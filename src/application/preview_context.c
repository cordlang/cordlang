#include "application/preview_internal.h"

 void preview_log_compile_error(PreviewCtx *ctx, const char *rel, int line,
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

 char *read_entry_from_config(const char *project_dir) {
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

 void load_meta(PreviewCtx *ctx) {
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

 int exists_under(const char *root, const char *rel) {
  char *p = fs_join(root, rel);
  if (!p) return 0;
  int ok = fs_exists(p);
  free(p);
  return ok;
}

 int file_mtime_size(const char *abs, long long *mtime, long long *size) {
  struct stat st;
  if (!abs || stat(abs, &st) != 0) return -1;
  *mtime = (long long)st.st_mtime;
  *size = (long long)st.st_size;
  return 0;
}

 void cache_clear(PreviewCtx *ctx) {
  for (int i = 0; i < ctx->n_cache; i++) {
    free(ctx->cache[i].body);
    ctx->cache[i].body = NULL;
  }
  ctx->n_cache = 0;
}

 PreviewCacheEntry *cache_find(PreviewCtx *ctx, const char *key) {
  for (int i = 0; i < ctx->n_cache; i++)
    if (strcmp(ctx->cache[i].key, key) == 0) return &ctx->cache[i];
  return NULL;
}

 void cache_put(PreviewCtx *ctx, const char *key, long long mtime,
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


 int preview_ctx_init(PreviewCtx *ctx, const char *dir) {
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

 void preview_ctx_free(PreviewCtx *ctx) { cache_clear(ctx); }

