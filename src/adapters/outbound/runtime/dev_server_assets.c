#include "adapters/outbound/runtime/dev_server.h"
#include "adapters/outbound/process/process_spawn.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
void dev_server_open_browser(const char *url) {
  if (!url) return;
#ifdef _WIN32
  ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
  char *argv[] = {"open", (char *)url, NULL};
  (void)process_run(NULL, argv, 0);
#else
  char *argv[] = {"xdg-open", (char *)url, NULL};
  if (process_run(NULL, argv, 0) != 0) {
    char *argv2[] = {"sensible-browser", (char *)url, NULL};
    (void)process_run(NULL, argv2, 0);
  }
#endif
}

/* ── mime ───────────────────────────────────────────────── */

static int ends_with(const char *s, const char *suffix) {
  size_t ls = strlen(s), lx = strlen(suffix);
  if (ls < lx) return 0;
  return strcmp(s + (ls - lx), suffix) == 0;
}

const char *dev_server_mime_for(const char *path) {
  if (!path) return "application/octet-stream";
  if (ends_with(path, ".html") || ends_with(path, ".htm")) return "text/html; charset=utf-8";
  if (ends_with(path, ".js") || ends_with(path, ".mjs")) return "text/javascript; charset=utf-8";
  if (ends_with(path, ".css")) return "text/css; charset=utf-8";
  if (ends_with(path, ".json") || ends_with(path, ".webmanifest")) return "application/json; charset=utf-8";
  if (ends_with(path, ".svg")) return "image/svg+xml";
  if (ends_with(path, ".png")) return "image/png";
  if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return "image/jpeg";
  if (ends_with(path, ".gif")) return "image/gif";
  if (ends_with(path, ".webp")) return "image/webp";
  if (ends_with(path, ".avif")) return "image/avif";
  if (ends_with(path, ".ico")) return "image/x-icon";
  if (ends_with(path, ".woff2")) return "font/woff2";
  if (ends_with(path, ".woff")) return "font/woff";
  if (ends_with(path, ".ttf")) return "font/ttf";
  if (ends_with(path, ".otf")) return "font/otf";
  if (ends_with(path, ".mp4")) return "video/mp4";
  if (ends_with(path, ".webm")) return "video/webm";
  if (ends_with(path, ".txt") || ends_with(path, ".md")) return "text/plain; charset=utf-8";
  if (ends_with(path, ".map")) return "application/json; charset=utf-8";
  return "application/octet-stream";
}

/* ── socket io ──────────────────────────────────────────── */

