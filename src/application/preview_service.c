/*
 * Dev-server request routing for the native ESM preview.
 *
 * URL map:
 *   /                      → shell that statically imports the entry .cord
 *   any *.cord path        → compiled on demand, served as text/javascript
 *   /@cord/runtime.js      → the client runtime
 *   /@cord/styles.css      → theme + base (single request)
 *   /@cord/base.css        → utilities for the classes this project emits
 *   /@cord/theme.css       → CSS vars from `theme` blocks (+ @font-face)
 *   /@cord/fonts/<file>    → cached WOFF2 from theme font: tokens
 *   /@cord/client          → reload client (Vite-style; alias: /@cord/hmr.js)
 *   /@cord/hmr             → SSE stream (dev_server.c)
 *   /<anything else>       → public/ then project root, else the shell (SPA)
 *
 * Every .cord is parsed ALONE (compiler_parse_file, not compiler_parse_project),
 * so its `use` / `route` targets stay module refs and become real imports. That
 * is what makes the browser walk the graph instead of receiving one blob.
 */
#include "application/preview_service.h"
#include "application/check_service.h"
#include "application/compile_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/fonts/font_cache.h"
#include "adapters/outbound/runtime/dev_server.h"
#include "adapters/outbound/runtime/preview_server.h"
#include "adapters/outbound/json/json_mini.h"
#include "adapters/outbound/term/term_log.h"
#include "domain/diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PREVIEW_CACHE_MAX 256

typedef struct {
  char key[512];
  long long mtime;
  long long size;
  unsigned long long bust;
  char *body;
  size_t len;
  char ctype[80];
} PreviewCacheEntry;

typedef struct {
  char root[1024];
  char root_abs[1024];
  char entry_abs[1024];
  char entry_url[512];
  char lang[32];
  char title[256];
  int has_site_css, has_site_js, has_favicon, has_logo_svg;
  unsigned long long bust;
  PreviewCacheEntry cache[PREVIEW_CACHE_MAX];
  int n_cache;
} PreviewCtx;

static void preview_log_compile_error(PreviewCtx *ctx, const char *rel, int line,
                                      int col, const char *msg) {
  static char last_key[700];
  static unsigned long long last_bust;
  char key[700];
  snprintf(key, sizeof(key), "%llu|%s|%d|%d|%s",
           (unsigned long long)(ctx ? ctx->bust : 0), rel ? rel : "?", line, col,
           msg ? msg : "");
  if (ctx && last_bust == ctx->bust && strcmp(last_key, key) == 0) return;
  last_bust = ctx ? ctx->bust : 0;
  snprintf(last_key, sizeof(last_key), "%s", key);
  if (line > 0)
    term_error("%s:%d:%d  %s", rel ? rel : "?", line, col, msg ? msg : "error");
  else
    term_error("%s  %s", rel ? rel : "?", msg ? msg : "error");
}

static char *read_entry_from_config(const char *project_dir) {
  char *cfg = fs_join(project_dir, "cordlang.json");
  if (!cfg) return NULL;
  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return NULL;
  char *entry = json_object_get_string(json, "entry");
  free(json);
  if (!entry) entry = strdup("src/app.cord");
  return entry;
}

static void load_meta(PreviewCtx *ctx) {
  snprintf(ctx->lang, sizeof(ctx->lang), "en");
  snprintf(ctx->title, sizeof(ctx->title), "Cordlang App");
  char *cfg = fs_join(ctx->root, "cordlang.json");
  if (!cfg) return;
  size_t len = 0;
  char *json = fs_read_file(cfg, &len);
  free(cfg);
  if (!json) return;
  char buf[256];
  if (json_object_copy_string(json, "lang", buf, sizeof(buf)))
    snprintf(ctx->lang, sizeof(ctx->lang), "%s", buf);
  if (json_object_copy_string(json, "title", buf, sizeof(buf)))
    snprintf(ctx->title, sizeof(ctx->title), "%s", buf);
  else if (json_object_copy_string(json, "name", buf, sizeof(buf)))
    snprintf(ctx->title, sizeof(ctx->title), "%s", buf);
  free(json);
}

static int exists_under(const char *root, const char *rel) {
  char *p = fs_join(root, rel);
  if (!p) return 0;
  int ok = fs_exists(p);
  free(p);
  return ok;
}

static int file_mtime_size(const char *abs, long long *mtime, long long *size) {
  struct stat st;
  if (!abs || stat(abs, &st) != 0) return -1;
  *mtime = (long long)st.st_mtime;
  *size = (long long)st.st_size;
  return 0;
}

static void cache_clear(PreviewCtx *ctx) {
  for (int i = 0; i < ctx->n_cache; i++) {
    free(ctx->cache[i].body);
    ctx->cache[i].body = NULL;
  }
  ctx->n_cache = 0;
}

static PreviewCacheEntry *cache_find(PreviewCtx *ctx, const char *key) {
  for (int i = 0; i < ctx->n_cache; i++)
    if (strcmp(ctx->cache[i].key, key) == 0) return &ctx->cache[i];
  return NULL;
}

static void cache_put(PreviewCtx *ctx, const char *key, long long mtime,
                      long long size, const char *ctype, char *body,
                      size_t len) {
  if (!body) return;
  PreviewCacheEntry *e = cache_find(ctx, key);
  if (!e) {
    if (ctx->n_cache >= PREVIEW_CACHE_MAX) {
      /* Drop oldest slot. */
      free(ctx->cache[0].body);
      memmove(ctx->cache, ctx->cache + 1,
              sizeof(PreviewCacheEntry) * (PREVIEW_CACHE_MAX - 1));
      ctx->n_cache = PREVIEW_CACHE_MAX - 1;
    }
    e = &ctx->cache[ctx->n_cache++];
    memset(e, 0, sizeof(*e));
    snprintf(e->key, sizeof(e->key), "%s", key);
  } else {
    free(e->body);
  }
  e->mtime = mtime;
  e->size = size;
  e->bust = ctx->bust;
  e->body = body;
  e->len = len;
  snprintf(e->ctype, sizeof(e->ctype), "%s", ctype ? ctype : "text/plain");
}

static int respond_str(DevResponse *out, int status, const char *ctype,
                       char *heap_body, size_t len, int no_store) {
  out->status = status;
  out->content_type = ctype;
  out->body = heap_body;
  out->len = len;
  out->no_store = no_store;
  out->cache_control = NULL;
  return 0;
}

static int respond_str_cached(DevResponse *out, int status, const char *ctype,
                              char *heap_body, size_t len,
                              const char *cache_control) {
  out->status = status;
  out->content_type = ctype;
  out->body = heap_body;
  out->len = len;
  out->no_store = 0;
  out->cache_control = cache_control;
  return 0;
}

static int respond_static_str(DevResponse *out, int status, const char *ctype,
                              const char *body, int no_store) {
  char *copy = strdup(body ? body : "");
  if (!copy) return -1;
  return respond_str(out, status, ctype, copy, strlen(copy), no_store);
}

static int respond_cached_copy(DevResponse *out, PreviewCacheEntry *e) {
  char *copy = malloc(e->len + 1);
  if (!copy) return -1;
  memcpy(copy, e->body, e->len);
  copy[e->len] = '\0';
  return respond_str(out, 200, e->ctype, copy, e->len, 1);
}

static int respond_not_found(DevResponse *out, const char *path) {
  char msg[1200];
  snprintf(msg, sizeof(msg), "404 %s", path ? path : "");
  return respond_static_str(out, 404, "text/plain; charset=utf-8", msg, 1);
}

static int respond_shell(PreviewCtx *ctx, DevResponse *out) {
  char *html = esm_index_html(ctx->lang, ctx->title, ctx->entry_url,
                              ctx->has_site_css, ctx->has_site_js,
                              ctx->has_favicon, ctx->has_logo_svg, 1);
  if (!html) return -1;
  return respond_str(out, 200, "text/html; charset=utf-8", html, strlen(html), 1);
}

static int respond_project_css(PreviewCtx *ctx, int want_theme, DevResponse *out) {
  const char *key = want_theme ? "@theme.css" : "@base.css";
  PreviewCacheEntry *hit = cache_find(ctx, key);
  if (hit && hit->bust == ctx->bust)
    return respond_cached_copy(out, hit);

  CompileResult r = compiler_parse_project(ctx->entry_abs);
  if (!r.ok || !r.ast) {
    compiler_result_free(&r);
    char *css = esm_base_css(NULL);
    if (!css) css = strdup("/* cordlang: base unavailable */\n");
    size_t len = strlen(css);
    char *owned = css;
    cache_put(ctx, key, 0, 0, "text/css; charset=utf-8", strdup(owned), len);
    free(owned);
    hit = cache_find(ctx, key);
    if (hit) return respond_cached_copy(out, hit);
    return -1;
  }
  IrProgram *ir = ir_from_ast(r.ast->root, ctx->entry_abs);
  char *css = NULL;
  if (ir) {
    if (want_theme) {
      ThemeCssOpts opts;
      memset(&opts, 0, sizeof(opts));
      opts.font_url_prefix = "/@cord/fonts";
      opts.resolve_fonts = 1;
      css = theme_css_generate_from_ir_opts(ir, &opts);
    } else {
      css = esm_base_css(ir);
    }
  }
  if (ir) ir_free(ir);
  compiler_result_free(&r);
  if (!css) return -1;
  size_t len = strlen(css);
  char *owned = css;
  cache_put(ctx, key, 0, 0, "text/css; charset=utf-8", strdup(owned), len);
  free(owned);
  hit = cache_find(ctx, key);
  if (hit) return respond_cached_copy(out, hit);
  return -1;
}

static int respond_styles_css(PreviewCtx *ctx, DevResponse *out) {
  const char *key = "@styles.css";
  PreviewCacheEntry *hit = cache_find(ctx, key);
  if (hit && hit->bust == ctx->bust)
    return respond_cached_copy(out, hit);

  CompileResult r = compiler_parse_project(ctx->entry_abs);
  if (!r.ok || !r.ast) {
    compiler_result_free(&r);
    /* Still serve base+overlay CSS so the error overlay is styled. */
    char *base = esm_base_css(NULL);
    if (!base) base = strdup("/* cordlang: base unavailable */\n");
    size_t len = strlen(base);
    cache_put(ctx, key, 0, 0, "text/css; charset=utf-8", strdup(base), len);
    free(base);
    hit = cache_find(ctx, key);
    if (hit) return respond_cached_copy(out, hit);
    return -1;
  }
  IrProgram *ir = ir_from_ast(r.ast->root, ctx->entry_abs);
  char *theme = NULL;
  char *base = NULL;
  if (ir) {
    ThemeCssOpts opts;
    memset(&opts, 0, sizeof(opts));
    opts.font_url_prefix = "/@cord/fonts";
    opts.resolve_fonts = 1;
    theme = theme_css_generate_from_ir_opts(ir, &opts);
    base = esm_base_css(ir);
    ir_free(ir);
  }
  compiler_result_free(&r);
  if (!theme) theme = strdup("/* theme */\n");
  if (!base) base = esm_base_css(NULL);
  if (!base) base = strdup("/* base */\n");
  size_t n = strlen(theme) + strlen(base) + 8;
  char *css = malloc(n);
  if (!css) {
    free(theme);
    free(base);
    return -1;
  }
  snprintf(css, n, "%s\n%s", theme, base);
  free(theme);
  free(base);
  size_t len = strlen(css);
  cache_put(ctx, key, 0, 0, "text/css; charset=utf-8", strdup(css), len);
  free(css);
  hit = cache_find(ctx, key);
  if (hit) return respond_cached_copy(out, hit);
  return -1;
}

static int respond_cord_font(const char *path, DevResponse *out) {
  /* /@cord/fonts/<slug-weight>.woff2 */
  const char *prefix = "/@cord/fonts/";
  if (strncmp(path, prefix, strlen(prefix)) != 0) return -1;
  const char *name = path + strlen(prefix);
  if (!*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, ".."))
    return respond_not_found(out, path);
  char *abs = font_cache_path_if_exists(name);
  if (!abs) return respond_not_found(out, path);
  size_t len = 0;
  char *body = fs_read_file(abs, &len);
  free(abs);
  if (!body) return -1;
  return respond_str_cached(out, 200, "font/woff2", body, len,
                            "public, max-age=31536000, immutable");
}

static int path_has_dotdot(const char *p) {
  return p && (strstr(p, "..") != NULL);
}

static int same_file(const char *a, const char *b) {
  char *na = fs_norm_path(a);
  char *nb = fs_norm_path(b);
  int eq = 0;
  if (na && nb) {
#ifdef _WIN32
    size_t i = 0;
    eq = 1;
    for (; na[i] && nb[i]; i++) {
      char ca = na[i], cb = nb[i];
      if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
      if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
      if (ca != cb) {
        eq = 0;
        break;
      }
    }
    if (eq && (na[i] || nb[i])) eq = 0;
#else
    eq = strcmp(na, nb) == 0;
#endif
  }
  free(na);
  free(nb);
  return eq;
}

static char *bust_query(PreviewCtx *ctx, char *buf, size_t n) {
  snprintf(buf, n, "?v=%llu", (unsigned long long)ctx->bust);
  return buf;
}

static int respond_cord_module(PreviewCtx *ctx, const char *path,
                               DevResponse *out) {
  if (path_has_dotdot(path)) return respond_not_found(out, path);

  const char *rel = path;
  while (*rel == '/') rel++;

  char *abs = fs_join(ctx->root, rel);
  if (!abs) return -1;
  if (!fs_exists(abs) || fs_is_dir(abs)) {
    free(abs);
    return respond_not_found(out, path);
  }

  long long mtime = 0, size = 0;
  file_mtime_size(abs, &mtime, &size);

  PreviewCacheEntry *hit = cache_find(ctx, path);
  if (hit && hit->mtime == mtime && hit->size == size && hit->bust == ctx->bust) {
    free(abs);
    return respond_cached_copy(out, hit);
  }

  CompileResult r = compiler_parse_file(abs);
  if (!r.ok || !r.ast) {
    DiagList diags;
    diag_list_init(&diags);
    diag_emit(&diags, DIAG_ERROR, rel,
              r.error_line > 0 ? r.error_line : 0,
              r.error_col > 0 ? r.error_col : 0, "%s",
              r.error ? r.error : "error de parseo");
    preview_log_compile_error(ctx, rel,
                              r.error_line > 0 ? r.error_line : 0,
                              r.error_col > 0 ? r.error_col : 0,
                              r.error ? r.error : "error de parseo");
    size_t src_len = 0;
    char *src = fs_read_file(abs, &src_len);
    compiler_result_free(&r);
    free(abs);
    char *mod = esm_error_module_from_diags(&diags, src, src_len);
    free(src);
    diag_list_free(&diags);
    if (!mod) return -1;
    return respond_str(out, 200, "text/javascript; charset=utf-8", mod,
                       strlen(mod), 1);
  }

  int is_entry = same_file(abs, ctx->entry_abs);

  char export_name[128];
  export_name[0] = '\0';
  if (!is_entry) {
    char *en = compiler_module_export_name(rel, NULL);
    if (en) {
      snprintf(export_name, sizeof(export_name), "%s", en);
      free(en);
    }
    compiler_wrap_module_body(r.ast->root, export_name, abs);
  }

  {
    DiagList diags;
    diag_list_init(&diags);
    if (check_service_on_module(r.ast->root, rel, &diags) != 0) {
      const Diagnostic *d0 = diags.len ? &diags.items[0] : NULL;
      preview_log_compile_error(
          ctx, rel, d0 ? d0->line : 0, d0 ? d0->col : 0,
          d0 && d0->message ? d0->message : "check failed");
      size_t src_len = 0;
      char *src = r.ast->source ? strdup(r.ast->source) : fs_read_file(abs, &src_len);
      if (r.ast->source && src) src_len = strlen(src);
      compiler_result_free(&r);
      free(abs);
      char *mod = esm_error_module_from_diags(&diags, src, src_len);
      free(src);
      diag_list_free(&diags);
      if (!mod) return -1;
      return respond_str(out, 200, "text/javascript; charset=utf-8", mod,
                         strlen(mod), 1);
    }
    diag_list_free(&diags);
  }

  IrProgram *ir = ir_from_ast(r.ast->root, abs);
  if (!ir) {
    compiler_result_free(&r);
    free(abs);
    return -1;
  }

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = is_entry ? ESM_MOD_ENTRY : ESM_MOD_COMPONENT;
  mod.export_name = export_name[0] ? export_name : NULL;
  mod.source_rel = rel;
  mod.abs_path = abs;
  mod.project_root = ctx->root_abs[0] ? ctx->root_abs : ctx->root;
  /* Bust imports so soft remount re-fetches the graph without full reload. */
  {
    char qbuf[64];
    mod.import_query = bust_query(ctx, qbuf, sizeof(qbuf));
    mod.truncated = 0;

    char *js = esm_generate_module(ir, &mod);
    ir_free(ir);
    compiler_result_free(&r);

    if (mod.truncated)
      fprintf(stderr,
              "cordlang preview: import/foreign limit reached in %s (some "
              "modules omitted)\n",
              rel);

    free(abs);
    if (!js) return -1;

    size_t len = strlen(js);
    cache_put(ctx, path, mtime, size, "text/javascript; charset=utf-8",
              strdup(js), len);
    free(js);
    hit = cache_find(ctx, path);
    if (hit) return respond_cached_copy(out, hit);
    return -1;
  }
}

static int respond_file(const char *abs, const char *path, DevResponse *out) {
  size_t len = 0;
  char *body = fs_read_file(abs, &len);
  if (!body) return -1;
  return respond_str(out, 200, dev_server_mime_for(path), body, len, 1);
}

static int try_static(PreviewCtx *ctx, const char *path, DevResponse *out) {
  if (path_has_dotdot(path)) return 1;
  const char *rel = path;
  while (*rel == '/') rel++;
  if (!*rel) return 1;

  char *pub = fs_join(ctx->root, "public");
  if (pub) {
    char *cand = fs_join(pub, rel);
    free(pub);
    if (cand) {
      if (fs_exists(cand) && !fs_is_dir(cand)) {
        int rc = respond_file(cand, path, out);
        free(cand);
        return rc == 0 ? 0 : 1;
      }
      free(cand);
    }
  }

  char *cand = fs_join(ctx->root, rel);
  if (cand) {
    if (fs_exists(cand) && !fs_is_dir(cand)) {
      int rc = respond_file(cand, path, out);
      free(cand);
      return rc == 0 ? 0 : 1;
    }
    free(cand);
  }
  return 1;
}

static int ends_with_cord(const char *p) {
  size_t n = p ? strlen(p) : 0;
  return n > 5 && strcmp(p + n - 5, ".cord") == 0;
}

static int preview_handler(const char *method, const char *path, void *userdata,
                           DevResponse *out) {
  (void)method;
  PreviewCtx *ctx = (PreviewCtx *)userdata;
  if (!path) return -1;

  if (strcmp(path, "/") == 0) return respond_shell(ctx, out);

  if (strcmp(path, "/@cord/runtime.js") == 0) {
    char *copy = strdup(esm_runtime_js());
    if (!copy) return -1;
    return respond_str_cached(out, 200, "text/javascript; charset=utf-8", copy,
                              strlen(copy),
                              "public, max-age=31536000, immutable");
  }

  if (strcmp(path, "/@cord/client") == 0 || strcmp(path, "/@cord/hmr.js") == 0)
    return respond_static_str(out, 200, "text/javascript; charset=utf-8",
                              esm_hmr_client_js(ctx->entry_url), 1);

  if (strcmp(path, "/@cord/styles.css") == 0)
    return respond_styles_css(ctx, out);

  if (strcmp(path, "/@cord/theme.css") == 0)
    return respond_project_css(ctx, 1, out);

  if (strcmp(path, "/@cord/base.css") == 0)
    return respond_project_css(ctx, 0, out);

  if (strncmp(path, "/@cord/fonts/", 13) == 0)
    return respond_cord_font(path, out);

  if (ends_with_cord(path)) return respond_cord_module(ctx, path, out);

  if (try_static(ctx, path, out) == 0) return 0;

  {
    const char *last_slash = strrchr(path, '/');
    const char *seg = last_slash ? last_slash + 1 : path;
    if (strchr(seg, '.') == NULL) return respond_shell(ctx, out);
  }
  return respond_not_found(out, path);
}

static void on_watch_change(void *userdata, const char *changed_url,
                            int full_reload) {
  PreviewCtx *ctx = (PreviewCtx *)userdata;
  ctx->bust = dev_server_project_stamp(ctx->root);
  cache_clear(ctx);
  (void)changed_url;
  (void)full_reload;
}

static int preview_ctx_init(PreviewCtx *ctx, const char *dir) {
  memset(ctx, 0, sizeof(*ctx));
  snprintf(ctx->root, sizeof(ctx->root), "%s", dir);

  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel ? entry_rel : "src/app.cord");
  if (!entry || !fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry ? entry : "?");
    free(entry_rel);
    free(entry);
    return 1;
  }
  snprintf(ctx->entry_abs, sizeof(ctx->entry_abs), "%s", entry);
  {
    char *norm = fs_norm_path(entry_rel ? entry_rel : "src/app.cord");
    snprintf(ctx->entry_url, sizeof(ctx->entry_url), "/%s",
             norm ? norm : "src/app.cord");
    free(norm);
  }
  free(entry_rel);
  free(entry);

  {
    char *root_abs = compiler_project_root(ctx->entry_abs);
    if (root_abs) {
      snprintf(ctx->root_abs, sizeof(ctx->root_abs), "%s", root_abs);
      free(root_abs);
    }
  }

  load_meta(ctx);
  ctx->has_site_css = exists_under(dir, "public/site.css");
  ctx->has_site_js = exists_under(dir, "public/site.js");
  ctx->has_favicon = exists_under(dir, "public/favicon.ico");
  ctx->has_logo_svg = exists_under(dir, "public/logo.svg");
  ctx->bust = dev_server_project_stamp(dir);
  return 0;
}

static void preview_ctx_free(PreviewCtx *ctx) { cache_clear(ctx); }

int preview_service_run(const char *project_dir, int open_browser) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  char *cfg = fs_join(dir, "cordlang.json");
  if (!cfg || !fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    fprintf(stderr, "Run: cordlang init\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  PreviewCtx *ctx = calloc(1, sizeof(PreviewCtx));
  if (!ctx) return 1;
  if (preview_ctx_init(ctx, dir) != 0) {
    free(ctx);
    return 1;
  }

  {
    CompileResult probe = compiler_parse_project(ctx->entry_abs);
    if (!probe.ok || !probe.ast) {
      fprintf(stderr, "Error: %s\n",
              probe.error ? probe.error : "el proyecto no compila");
      compiler_result_free(&probe);
      preview_ctx_free(ctx);
      free(ctx);
      return 1;
    }
    compiler_result_free(&probe);
  }

  term_info("serving %s", ctx->entry_url);
  fflush(stdout);

  int rc = dev_server_serve(4173, dir, ctx->entry_url, open_browser,
                            preview_handler, ctx, on_watch_change);
  preview_ctx_free(ctx);
  free(ctx);
  return rc;
}

int preview_service_run_html(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  const BackendPort *backend = backend_find("html");
  if (!backend) {
    fprintf(stderr, "Error: HTML runtime backend not registered\n");
    return 1;
  }

  char *cfg = fs_join(dir, "cordlang.json");
  if (!fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    fprintf(stderr, "Run: cordlang init\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel ? entry_rel : "src/app.cord");
  free(entry_rel);

  if (!entry || !fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry ? entry : "?");
    free(entry);
    return 1;
  }

  printf("Compiling %s -> native HTML runtime...\n", entry);
  char *html = compile_service_file(entry, "html");
  free(entry);
  if (!html) return 1;

  if (backend->scaffold) backend->scaffold(dir, html);

  size_t html_len = strlen(html);
  int rc = preview_server_serve(html, html_len, 4173);
  free(html);
  return rc;
}

/* ── smoke ──────────────────────────────────────────────── */

static int smoke_check(PreviewCtx *ctx, const char *path, int want_status,
                       const char *want_ctype_sub, const char *want_body_sub) {
  DevResponse out;
  memset(&out, 0, sizeof(out));
  if (preview_handler("GET", path, ctx, &out) != 0) {
    fprintf(stderr, "smoke FAIL: handler error for %s\n", path);
    free(out.body);
    return 1;
  }
  int fail = 0;
  if (out.status != want_status) {
    fprintf(stderr, "smoke FAIL: %s status %d (want %d)\n", path, out.status,
            want_status);
    fail = 1;
  }
  if (want_ctype_sub &&
      (!out.content_type || !strstr(out.content_type, want_ctype_sub))) {
    fprintf(stderr, "smoke FAIL: %s content-type '%s' (want contains %s)\n", path,
            out.content_type ? out.content_type : "(null)", want_ctype_sub);
    fail = 1;
  }
  if (want_body_sub && (!out.body || !strstr(out.body, want_body_sub))) {
    fprintf(stderr, "smoke FAIL: %s body missing '%s'\n", path, want_body_sub);
    fail = 1;
  }
  free(out.body);
  return fail;
}

int preview_service_smoke(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  PreviewCtx ctx;
  if (preview_ctx_init(&ctx, dir) != 0) return 1;

  int failed = 0;
  failed += smoke_check(&ctx, "/", 200, "text/html", "script");
  failed += smoke_check(&ctx, ctx.entry_url, 200, "javascript", "export");
  failed += smoke_check(&ctx, "/@cord/runtime.js", 200, "javascript", "mount");
  failed += smoke_check(&ctx, "/@cord/base.css", 200, "text/css", NULL);
  failed += smoke_check(&ctx, "/no-such-asset.xyz", 404, "text/plain", "404");
  failed += smoke_check(&ctx, "/spa-route-without-ext", 200, "text/html", "app");

  /* R5: structured error overlay modules (parse + check). */
  {
    char bad_parse[1200];
    char bad_check[1200];
    snprintf(bad_parse, sizeof(bad_parse), "%s/src/__smoke_parse_err.cord",
             ctx.root);
    snprintf(bad_check, sizeof(bad_check), "%s/src/__smoke_check_err.cord",
             ctx.root);
    FILE *fp = fopen(bad_parse, "wb");
    if (fp) {
      fputs("foreign\n", fp);
      fclose(fp);
      failed += smoke_check(&ctx, "/src/__smoke_parse_err.cord", 200,
                            "javascript", "showCompileError");
      failed += smoke_check(&ctx, "/src/__smoke_parse_err.cord", 200,
                            "javascript", "line\\\"");
      remove(bad_parse);
    } else {
      fprintf(stderr, "smoke FAIL: cannot write parse-error fixture\n");
      failed++;
    }
    fp = fopen(bad_check, "wb");
    if (fp) {
      fputs("def Bad\n  col className=\"flex\"\n    p \"nope\"\n", fp);
      fclose(fp);
      failed += smoke_check(&ctx, "/src/__smoke_check_err.cord", 200,
                            "javascript", "showCompileError");
      failed += smoke_check(&ctx, "/src/__smoke_check_err.cord", 200,
                            "javascript", "jsx-attr");
      remove(bad_check);
    } else {
      fprintf(stderr, "smoke FAIL: cannot write check-error fixture\n");
      failed++;
    }
  }

  /* Entry with routes should emit a non-empty routes array when present. */
  {
    DevResponse out;
    memset(&out, 0, sizeof(out));
    if (preview_handler("GET", ctx.entry_url, &ctx, &out) == 0 && out.body) {
      if (strstr(out.body, "export const routes") &&
          strstr(out.body, "routes = [\n];")) {
        fprintf(stderr, "smoke FAIL: entry routes array is empty\n");
        failed++;
      }
    }
    free(out.body);
  }

  preview_ctx_free(&ctx);
  if (failed) {
    fprintf(stderr, "preview smoke: %d check(s) failed\n", failed);
    return 1;
  }
  printf("preview smoke: ok\n");
  return 0;
}

/* ── build esm ──────────────────────────────────────────── */

static int ends_with_ci(const char *s, const char *suf) {
  size_t ls = strlen(s), lx = strlen(suf);
  if (ls < lx) return 0;
  for (size_t i = 0; i < lx; i++) {
    char a = s[ls - lx + i], b = suf[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

static void cord_to_js_rel(const char *rel_cord, char *out, size_t n) {
  snprintf(out, n, "%s", rel_cord);
  size_t l = strlen(out);
  if (l > 5 && ends_with_ci(out, ".cord")) {
    out[l - 5] = '\0';
    strncat(out, ".js", n - strlen(out) - 1);
  }
}

static void rewrite_cord_imports_to_js(char *js) {
  /* Rewrite '/path/Foo.cord' → '/path/Foo.js' inside the emit. */
  char *p = js;
  while (p && *p) {
    char *hit = strstr(p, ".cord'");
    if (!hit) hit = strstr(p, ".cord\"");
    if (!hit) break;
    hit[1] = 'j';
    hit[2] = 's';
    /* shift: ".cord'" is 6 chars, ".js'" is 4 — compact */
    memmove(hit + 3, hit + 5, strlen(hit + 5) + 1);
    p = hit + 3;
  }
}

typedef struct {
  PreviewCtx *ctx;
  const char *out_root;
  int errors;
  int written;
} BuildWalk;

static int write_text(const char *path, const char *body) {
  char *dir = fs_dirname(path);
  if (dir) {
    fs_mkdir_p(dir);
    free(dir);
  }
  return fs_write_file(path, body ? body : "");
}

static void build_one_cord(const char *abs, const char *rel, void *ud) {
  BuildWalk *bw = (BuildWalk *)ud;
  PreviewCtx *ctx = bw->ctx;

  CompileResult r = compiler_parse_file(abs);
  if (!r.ok || !r.ast) {
    fprintf(stderr, "build esm: skip %s (%s)\n", rel,
            r.error ? r.error : "parse error");
    compiler_result_free(&r);
    bw->errors++;
    return;
  }

  int is_entry = same_file(abs, ctx->entry_abs);
  char export_name[128];
  export_name[0] = '\0';
  if (!is_entry) {
    char *en = compiler_module_export_name(rel, NULL);
    if (en) {
      snprintf(export_name, sizeof(export_name), "%s", en);
      free(en);
    }
    compiler_wrap_module_body(r.ast->root, export_name, abs);
  }

  IrProgram *ir = ir_from_ast(r.ast->root, abs);
  if (!ir) {
    compiler_result_free(&r);
    bw->errors++;
    return;
  }

  EsmModuleCtx mod;
  memset(&mod, 0, sizeof(mod));
  mod.kind = is_entry ? ESM_MOD_ENTRY : ESM_MOD_COMPONENT;
  mod.export_name = export_name[0] ? export_name : NULL;
  mod.source_rel = rel;
  mod.abs_path = abs;
  mod.project_root = ctx->root_abs[0] ? ctx->root_abs : ctx->root;

  char *js = esm_generate_module(ir, &mod);
  ir_free(ir);
  compiler_result_free(&r);
  if (!js) {
    bw->errors++;
    return;
  }
  if (mod.truncated)
    fprintf(stderr, "build esm: import limit truncated in %s\n", rel);

  rewrite_cord_imports_to_js(js);

  char js_rel[512];
  cord_to_js_rel(rel, js_rel, sizeof(js_rel));
  char *out_path = fs_join(bw->out_root, js_rel);
  if (!out_path || write_text(out_path, js) != 0) {
    fprintf(stderr, "build esm: cannot write %s\n", js_rel);
    bw->errors++;
  } else {
    bw->written++;
  }
  free(out_path);
  free(js);
}

int preview_service_build_esm(const char *project_dir) {
  const char *dir = project_dir && *project_dir ? project_dir : ".";
  backend_register_all();

  char *cfg = fs_join(dir, "cordlang.json");
  if (!cfg || !fs_exists(cfg)) {
    fprintf(stderr, "Error: not a Cordlang project (missing cordlang.json)\n");
    free(cfg);
    return 1;
  }
  free(cfg);

  PreviewCtx ctx;
  if (preview_ctx_init(&ctx, dir) != 0) return 1;

  {
    CompileResult probe = compiler_parse_project(ctx.entry_abs);
    if (!probe.ok || !probe.ast) {
      fprintf(stderr, "Error: %s\n",
              probe.error ? probe.error : "el proyecto no compila");
      compiler_result_free(&probe);
      preview_ctx_free(&ctx);
      return 1;
    }
    compiler_result_free(&probe);
  }

  char *out_root = fs_join(dir, "dist/esm");
  if (!out_root) {
    preview_ctx_free(&ctx);
    return 1;
  }
  fs_mkdir_p(out_root);

  /* Shell without HMR client; entry URL uses .js */
  char entry_js[512];
  {
    const char *e = ctx.entry_url;
    while (*e == '/') e++;
    cord_to_js_rel(e, entry_js, sizeof(entry_js));
  }
  char entry_url_js[520];
  snprintf(entry_url_js, sizeof(entry_url_js), "/%s", entry_js);

  char *html =
      esm_index_html(ctx.lang, ctx.title, entry_url_js, ctx.has_site_css,
                     ctx.has_site_js, ctx.has_favicon, ctx.has_logo_svg, 0);
  if (html) {
    char *hp = fs_join(out_root, "index.html");
    if (hp) {
      write_text(hp, html);
      free(hp);
    }
    free(html);
  }

  char *cord_dir = fs_join(out_root, "@cord");
  if (cord_dir) {
    fs_mkdir_p(cord_dir);
    free(cord_dir);
  }
  {
    char *p = fs_join(out_root, "@cord/runtime.js");
    if (p) {
      write_text(p, esm_runtime_js());
      free(p);
    }
  }

  {
    CompileResult r = compiler_parse_project(ctx.entry_abs);
    if (r.ok && r.ast) {
      IrProgram *ir = ir_from_ast(r.ast->root, ctx.entry_abs);
      if (ir) {
        ThemeCssOpts opts;
        memset(&opts, 0, sizeof(opts));
        opts.font_url_prefix = "/@cord/fonts";
        opts.resolve_fonts = 1;
        char *base = esm_base_css(ir);
        char *theme = theme_css_generate_from_ir_opts(ir, &opts);
        if (base) {
          char *p = fs_join(out_root, "@cord/base.css");
          if (p) {
            write_text(p, base);
            free(p);
          }
        }
        if (theme) {
          char *p = fs_join(out_root, "@cord/theme.css");
          if (p) {
            write_text(p, theme);
            free(p);
          }
        }
        if (theme && base) {
          size_t n = strlen(theme) + strlen(base) + 8;
          char *styles = malloc(n);
          if (styles) {
            snprintf(styles, n, "%s\n%s", theme, base);
            char *p = fs_join(out_root, "@cord/styles.css");
            if (p) {
              write_text(p, styles);
              free(p);
            }
            free(styles);
          }
        }
        free(base);
        free(theme);
        char *fonts_out = fs_join(out_root, "@cord/fonts");
        if (fonts_out) {
          font_cache_install_from_ir(ir, fonts_out);
          free(fonts_out);
        }
        ir_free(ir);
      }
    }
    compiler_result_free(&r);
  }

  /* Copy public/ to dist/esm root */
  {
    char *pub = fs_join(dir, "public");
    if (pub && fs_exists(pub) && fs_is_dir(pub)) {
      fs_copy_tree(pub, out_root);
    }
    free(pub);
  }

  BuildWalk bw;
  memset(&bw, 0, sizeof(bw));
  bw.ctx = &ctx;
  bw.out_root = out_root;
  fs_walk_cord(dir, build_one_cord, &bw);

  printf("build esm: wrote %d module(s) to %s\n", bw.written, out_root);
  if (bw.errors)
    fprintf(stderr, "build esm: %d file(s) failed\n", bw.errors);

  free(out_root);
  preview_ctx_free(&ctx);
  return bw.errors ? 1 : 0;
}
