#ifndef CORDLANG_PREVIEW_INTERNAL_H
#define CORDLANG_PREVIEW_INTERNAL_H

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
#include "domain/ir.h"
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

void preview_log_compile_error(PreviewCtx *ctx, const char *rel, int line,
                               int col, const char *msg);
char *read_entry_from_config(const char *project_dir);
void load_meta(PreviewCtx *ctx);
int exists_under(const char *root, const char *rel);
int file_mtime_size(const char *abs, long long *mtime, long long *size);
void cache_clear(PreviewCtx *ctx);
PreviewCacheEntry *cache_find(PreviewCtx *ctx, const char *key);
void cache_put(PreviewCtx *ctx, const char *key, long long mtime, long long size,
               const char *ctype, char *body, size_t len);
int respond_str(DevResponse *out, int status, const char *ctype,
                char *heap_body, size_t len, int no_store);
int respond_str_cached(DevResponse *out, int status, const char *ctype,
                       char *heap_body, size_t len, const char *cache_control);
int respond_static_str(DevResponse *out, int status, const char *ctype,
                       const char *body, int no_store);
int respond_cached_copy(DevResponse *out, PreviewCacheEntry *e);
int respond_not_found(DevResponse *out, const char *path);
int respond_shell(PreviewCtx *ctx, DevResponse *out);
int respond_project_css(PreviewCtx *ctx, int want_theme, DevResponse *out);
int respond_styles_css(PreviewCtx *ctx, DevResponse *out);
int respond_cord_font(const char *path, DevResponse *out);
int path_has_dotdot(const char *path);
int same_file(const char *a, const char *b);
char *bust_query(PreviewCtx *ctx, char *buf, size_t n);
int respond_cord_module(PreviewCtx *ctx, const char *path, DevResponse *out);
int preview_handler(const char *method, const char *path, void *userdata,
                    DevResponse *out);
void on_watch_change(void *userdata, const char *changed_url, int full_reload);
int preview_ctx_init(PreviewCtx *ctx, const char *dir);
void preview_ctx_free(PreviewCtx *ctx);

#endif