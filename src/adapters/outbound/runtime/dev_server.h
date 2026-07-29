#ifndef CORDLANG_DEV_SERVER_H
#define CORDLANG_DEV_SERVER_H

#include <stddef.h>

/*
 * Minimal HTTP/1.1 dev server for the native ESM preview.
 *
 * Split of duties: this file owns sockets, request parsing, the SSE reload
 * channel and file-change detection. It knows nothing about .cord — the caller's
 * handler decides what a path means and what to compile, so all filesystem and
 * compiler access stays in the application layer.
 *
 * preview_server.c stays as-is for the legacy static-HTML preview
 * (`cordlang run html`), which serves one fixed blob for every path.
 */

typedef struct {
  int status;               /* 200, 404, 500 … */
  const char *content_type; /* NULL → text/plain */
  char *body;               /* heap; the server frees it */
  size_t len;               /* 0 → strlen(body) */
  int no_store;             /* 1 → Cache-Control: no-store */
  /* If set, emitted as Cache-Control and overrides no_store. */
  const char *cache_control;
} DevResponse;

/*
 * Build a response for `path` (query string already stripped, percent-decoded).
 * Return 0 on success with `out` filled. Non-zero → the server answers 500.
 */
typedef int (*DevHandlerFn)(const char *method, const char *path, void *userdata,
                            DevResponse *out);

/*
 * Called when the watch stamp changes. `changed_url` is a site URL like
 * `/src/pages/Home.cord` when a single soft-updatable .cord changed, else NULL
 * (caller should full-reload). `full_reload` is 1 for entry / public / config /
 * multi-file changes.
 */
typedef void (*DevWatchFn)(void *userdata, const char *changed_url,
                           int full_reload);

/*
 * Serve until Ctrl+C. Binds 127.0.0.1 only, trying `port` then the next 20.
 * `watch_dir` is polled for .cord, public/, and cordlang.json changes.
 * Pass NULL watch_dir to disable watching. `on_change` may be NULL (SSE still
 * broadcasts reload/update). Returns 0 on clean shutdown.
 */
int dev_server_serve(int port, const char *watch_dir, const char *entry_label,
                     int open_browser, DevHandlerFn handler, void *userdata,
                     DevWatchFn on_change);

/* FNV-ish project stamp over watched files (for emit cache bust). */
unsigned long long dev_server_project_stamp(const char *watch_dir);

/* Best-effort browser open (shared with the legacy preview). */
void dev_server_open_browser(const char *url);

/* Content type for a static file path, by extension. Never NULL. */
const char *dev_server_mime_for(const char *path);

#endif
