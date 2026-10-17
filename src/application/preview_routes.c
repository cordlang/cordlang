#include "application/preview_internal.h"

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

 int preview_handler(const char *method, const char *path, void *userdata,
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

 void on_watch_change(void *userdata, const char *changed_url,
                            int full_reload) {
  PreviewCtx *ctx = (PreviewCtx *)userdata;
  ctx->bust = dev_server_project_stamp(ctx->root);
  cache_clear(ctx);
  (void)changed_url;
  (void)full_reload;
}

