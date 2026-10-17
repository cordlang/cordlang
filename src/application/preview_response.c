#include "application/preview_internal.h"

 int respond_str(DevResponse *out, int status, const char *ctype,
                       char *heap_body, size_t len, int no_store) {
  out->status = status;
  out->content_type = ctype;
  out->body = heap_body;
  out->len = len;
  out->no_store = no_store;
  out->cache_control = NULL;
  return 0;
}

 int respond_str_cached(DevResponse *out, int status, const char *ctype,
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

 int respond_static_str(DevResponse *out, int status, const char *ctype,
                              const char *body, int no_store) {
  char *copy = strdup(body ? body : "");
  if (!copy) return -1;
  return respond_str(out, status, ctype, copy, strlen(copy), no_store);
}

 int respond_cached_copy(DevResponse *out, PreviewCacheEntry *e) {
  char *copy = malloc(e->len + 1);
  if (!copy) return -1;
  memcpy(copy, e->body, e->len);
  copy[e->len] = '\0';
  return respond_str(out, 200, e->ctype, copy, e->len, 1);
}

 int respond_not_found(DevResponse *out, const char *path) {
  char msg[1200];
  snprintf(msg, sizeof(msg), "404 %s", path ? path : "");
  return respond_static_str(out, 404, "text/plain; charset=utf-8", msg, 1);
}

 int respond_shell(PreviewCtx *ctx, DevResponse *out) {
  char *html = esm_index_html(ctx->lang, ctx->title, ctx->entry_url,
                              ctx->has_site_css, ctx->has_site_js,
                              ctx->has_favicon, ctx->has_logo_svg, 1);
  if (!html) return -1;
  return respond_str(out, 200, "text/html; charset=utf-8", html, strlen(html), 1);
}

 int respond_project_css(PreviewCtx *ctx, int want_theme, DevResponse *out) {
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

 int respond_styles_css(PreviewCtx *ctx, DevResponse *out) {
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

 int respond_cord_font(const char *path, DevResponse *out) {
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

