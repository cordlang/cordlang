/*
 * Dev-server request routing for the native ESM preview.
 *
 * URL map:
 *   /                      → shell that statically imports the entry .cord
 *   any *.cord path        → compiled on demand, served as text/javascript
 *   /@cord/runtime.js      → the client runtime
 *   /@cord/base.css        → utilities for the classes this project emits
 *   /@cord/theme.css       → CSS vars from `theme` blocks
 *   /@cord/client          → reload client (Vite-style; alias: /@cord/hmr.js)
 *   /@cord/hmr             → SSE stream (dev_server.c)
 *   /<anything else>       → public/ then project root, else the shell (SPA)
 *
 * Every .cord is parsed ALONE (compiler_parse_file, not compiler_parse_project),
 * so its `use` / `route` targets stay module refs and become real imports. That
 * is what makes the browser walk the graph instead of receiving one blob.
 */
#include "application/preview_service.h"
#include "application/compile_service.h"
#include "application/ports/backend_port.h"
#include "application/ports/compiler_port.h"
#include "application/ports/fs_port.h"
#include "adapters/outbound/backends/esm/esm_backend.h"
#include "adapters/outbound/backends/theme_css.h"
#include "adapters/outbound/runtime/dev_server.h"
#include "adapters/outbound/runtime/preview_server.h"
#include "adapters/outbound/json/json_mini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char root[1024];       /* project dir as given */
  char root_abs[1024];   /* project root that jails module resolution */
  char entry_abs[1024];  /* absolute entry .cord */
  char entry_url[512];   /* /src/app.cord */
  char lang[32];
  char title[256];
  int has_site_css, has_site_js, has_favicon, has_logo_svg;
} PreviewCtx;

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

/* ── response helpers ───────────────────────────────────── */

static int respond_str(DevResponse *out, int status, const char *ctype,
                       char *heap_body, size_t len, int no_store) {
  out->status = status;
  out->content_type = ctype;
  out->body = heap_body;
  out->len = len;
  out->no_store = no_store;
  return 0;
}

static int respond_static_str(DevResponse *out, int status, const char *ctype,
                              const char *body, int no_store) {
  char *copy = strdup(body ? body : "");
  if (!copy) return -1;
  return respond_str(out, status, ctype, copy, strlen(copy), no_store);
}

static int respond_not_found(DevResponse *out, const char *path) {
  char msg[1200];
  snprintf(msg, sizeof(msg), "404 %s", path ? path : "");
  return respond_static_str(out, 404, "text/plain; charset=utf-8", msg, 1);
}

/* ── shell ──────────────────────────────────────────────── */

static int respond_shell(PreviewCtx *ctx, DevResponse *out) {
  char *html = esm_index_html(ctx->lang, ctx->title, ctx->entry_url,
                              ctx->has_site_css, ctx->has_site_js,
                              ctx->has_favicon, ctx->has_logo_svg);
  if (!html) return -1;
  return respond_str(out, 200, "text/html; charset=utf-8", html, strlen(html), 1);
}

/* ── whole-project passes (theme + utility CSS) ─────────── */

/*
 * theme.css and base.css need the FULL project: the theme block lives in the
 * entry, and utility classes come from every component. This is the one place
 * the flattened parse is still the right tool.
 */
static int respond_project_css(PreviewCtx *ctx, int want_theme, DevResponse *out) {
  CompileResult r = compiler_parse_project(ctx->entry_abs);
  if (!r.ok || !r.ast) {
    char msg[1024];
    snprintf(msg, sizeof(msg), "/* cordlang: no compila: %s */\n",
             r.error ? r.error : "error de parseo");
    compiler_result_free(&r);
    return respond_static_str(out, 200, "text/css; charset=utf-8", msg, 1);
  }
  IrProgram *ir = ir_from_ast(r.ast->root, ctx->entry_abs);
  char *css = NULL;
  if (ir) css = want_theme ? theme_css_generate_from_ir(ir) : esm_base_css(ir);
  if (ir) ir_free(ir);
  compiler_result_free(&r);
  if (!css) return -1;
  return respond_str(out, 200, "text/css; charset=utf-8", css, strlen(css), 1);
}

/* ── .cord → ES module ──────────────────────────────────── */

static int path_has_dotdot(const char *p) {
  return p && (strstr(p, "..") != NULL);
}

static int same_file(const char *a, const char *b) {
  char *na = fs_norm_path(a);
  char *nb = fs_norm_path(b);
  int eq = 0;
  if (na && nb) {
#ifdef _WIN32
    /* Windows paths are case-insensitive; compare folded. */
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

  CompileResult r = compiler_parse_file(abs);
  if (!r.ok || !r.ast) {
    char detail[900];
    snprintf(detail, sizeof(detail), "%s: %s", rel,
             r.error ? r.error : "error de parseo");
    compiler_result_free(&r);
    free(abs);
    char *mod = esm_error_module(detail);
    if (!mod) return -1;
    /* 200 so the browser executes the module and the overlay can render. */
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
    /* Body-only files (no `def`) become a component named after the file. */
    compiler_wrap_module_body(r.ast->root, export_name, abs);
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

  char *js = esm_generate_module(ir, &mod);
  ir_free(ir);
  compiler_result_free(&r);
  free(abs);

  if (!js) return -1;
  return respond_str(out, 200, "text/javascript; charset=utf-8", js, strlen(js), 1);
}

/* ── static assets ──────────────────────────────────────── */

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

  /* public/ first: that is what ships at the site root. */
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
  return 1; /* not found */
}

/* ── handler ────────────────────────────────────────────── */

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

  if (strcmp(path, "/@cord/runtime.js") == 0)
    return respond_static_str(out, 200, "text/javascript; charset=utf-8",
                              esm_runtime_js(), 1);

  /* /@cord/client ≈ Vite's /@vite/client; /@cord/hmr.js kept as alias. */
  if (strcmp(path, "/@cord/client") == 0 || strcmp(path, "/@cord/hmr.js") == 0)
    return respond_static_str(out, 200, "text/javascript; charset=utf-8",
                              esm_hmr_client_js(), 1);

  if (strcmp(path, "/@cord/theme.css") == 0)
    return respond_project_css(ctx, 1, out);

  if (strcmp(path, "/@cord/base.css") == 0)
    return respond_project_css(ctx, 0, out);

  if (ends_with_cord(path)) return respond_cord_module(ctx, path, out);

  if (try_static(ctx, path, out) == 0) return 0;

  /*
   * SPA fallback: an unknown extensionless path is a client route, so hand back
   * the shell and let the router match it. Anything with an extension is a
   * genuine missing asset and should 404 loudly instead of returning HTML.
   */
  {
    const char *last_slash = strrchr(path, '/');
    const char *seg = last_slash ? last_slash + 1 : path;
    if (strchr(seg, '.') == NULL) return respond_shell(ctx, out);
  }
  return respond_not_found(out, path);
}

/* ── entry points ───────────────────────────────────────── */

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
  snprintf(ctx->root, sizeof(ctx->root), "%s", dir);

  char *entry_rel = read_entry_from_config(dir);
  char *entry = fs_join(dir, entry_rel ? entry_rel : "src/app.cord");
  if (!entry || !fs_exists(entry)) {
    fprintf(stderr, "Error: entry file not found: %s\n", entry ? entry : "?");
    free(entry_rel);
    free(entry);
    free(ctx);
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

  /* Fail fast on a broken project instead of showing a blank tab. */
  {
    CompileResult probe = compiler_parse_project(ctx->entry_abs);
    if (!probe.ok || !probe.ast) {
      fprintf(stderr, "Error: %s\n",
              probe.error ? probe.error : "el proyecto no compila");
      compiler_result_free(&probe);
      free(ctx);
      return 1;
    }
    compiler_result_free(&probe);
  }

  printf("Sirviendo %s como modulos ES nativos...\n", ctx->entry_url);
  fflush(stdout);

  int rc = dev_server_serve(4173, dir, ctx->entry_url, open_browser,
                            preview_handler, ctx);
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
