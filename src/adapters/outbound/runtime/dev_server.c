/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/dev_server.h"
#include "adapters/outbound/runtime/dev_server_internal.h"
#include "adapters/outbound/runtime/dev_watch_internal.h"
#include "adapters/outbound/term/term_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile int g_running = 1;
#ifdef _WIN32
static BOOL WINAPI on_console_ctrl(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
      type == CTRL_CLOSE_EVENT) {
    g_running = 0;
    return TRUE;
  }
  return FALSE;
}
#else
static void on_sigint(int sig) {
  (void)sig;
  g_running = 0;
}
#endif
/*
 * Monotonic wall-clock milliseconds. NOT clock(): that is CPU time on POSIX,
 * and this loop spends nearly all its time blocked in select(), so a clock()
 * based interval would almost never elapse and the watcher would go dead
 * outside Windows.
 */
static unsigned long long now_ms(void) {
#ifdef _WIN32
  return (unsigned long long)GetTickCount64();
#else
  struct timespec ts;
#if defined(CLOCK_MONOTONIC)
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    return (unsigned long long)ts.tv_sec * 1000ULL +
           (unsigned long long)(ts.tv_nsec / 1000000L);
#endif
  return (unsigned long long)time(NULL) * 1000ULL;
#endif
}

int dev_server_serve(int port, const char *watch_dir, const char *entry_label,
                     int open_browser, DevHandlerFn handler, void *userdata,
                     DevWatchFn on_change) {
  if (!handler) return 1;
  if (port <= 0) port = 4173;

#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    fprintf(stderr, "Error: WSAStartup failed\n");
    return 1;
  }
  SetConsoleCtrlHandler(on_console_ctrl, TRUE);
#else
  signal(SIGINT, on_sigint);
  signal(SIGTERM, on_sigint);
  signal(SIGPIPE, SIG_IGN);
#endif

  SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server == INVALID_SOCKET) {
    fprintf(stderr, "Error: cannot create socket\n");
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 only */

  int bound = 0;
  for (int p = port; p < port + 20; p++) {
    addr.sin_port = htons((unsigned short)p);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) != SOCK_ERR) {
      port = p;
      bound = 1;
      break;
    }
  }
  if (!bound) {
    fprintf(stderr, "Error: cannot bind dev server (ports %d-%d busy)\n", port,
            port + 19);
    CLOSESOCK(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  if (listen(server, 16) == SOCK_ERR) {
    fprintf(stderr, "Error: listen failed\n");
    CLOSESOCK(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  char url[128];
  snprintf(url, sizeof(url), "http://127.0.0.1:%d", port);
  term_banner_preview(url, entry_label, watch_dir != NULL);
  if (open_browser) dev_server_open_browser(url);

  SseSet sse;
  memset(&sse, 0, sizeof(sse));
  for (int i = 0; i < MAX_SSE; i++) sse.socks[i] = INVALID_SOCKET;
  sse.count = 0;

  WatchSnapshot snap = dev_watch_take_snapshot(watch_dir);
  unsigned long long last_check = now_ms();

  g_running = 1;
  while (g_running) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(server, &rfds);
    SOCKET maxfd = server;
    for (int i = 0; i < sse.count; i++) {
      FD_SET(sse.socks[i], &rfds);
      if (sse.socks[i] > maxfd) maxfd = sse.socks[i];
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000; /* keep Ctrl+C responsive */

    int sel = select((int)maxfd + 1, &rfds, NULL, NULL, &tv);
    if (sel == SOCK_ERR) {
#ifndef _WIN32
      if (errno == EINTR) continue;
#endif
      if (!g_running) break;
      break;
    }

    /* A readable SSE socket means the tab went away. */
    for (int i = 0; i < sse.count;) {
      if (FD_ISSET(sse.socks[i], &rfds)) {
        char sink[256];
#ifdef _WIN32
        int n = recv(sse.socks[i], sink, (int)sizeof(sink), 0);
#else
        ssize_t n = recv(sse.socks[i], sink, sizeof(sink), 0);
#endif
        if (n <= 0) {
          CLOSESOCK(sse.socks[i]);
          memmove(sse.socks + i, sse.socks + i + 1,
                  sizeof(SOCKET) * (size_t)(sse.count - i - 1));
          sse.count--;
          continue;
        }
      }
      i++;
    }

    if (watch_dir) {
      unsigned long long tnow = now_ms();
      if (tnow - last_check > 400ULL) {
        last_check = tnow;
        WatchSnapshot now = dev_watch_take_snapshot(watch_dir);
        if (!dev_watch_stamp_equal(&snap.stamp, &now.stamp)) {
          char changed_url[560];
          int full = dev_watch_classify_change(&snap, &now, entry_label, changed_url,
                                     sizeof(changed_url));
          snap = now;
          if (on_change)
            on_change(userdata, full ? NULL : changed_url, full);
          if (full) {
            term_info("reload  %s", changed_url[0] ? changed_url : "project");
            sse_broadcast(&sse, "reload");
          } else {
            char ev[600];
            snprintf(ev, sizeof(ev), "update:%s", changed_url);
            term_info("update  %s", changed_url);
            sse_broadcast(&sse, ev);
          }
        }
      }
    }

    if (sel == 0 || !FD_ISSET(server, &rfds)) continue;

    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    SOCKET client = accept(server, (struct sockaddr *)&caddr, &clen);
    if (client == INVALID_SOCKET) continue;

    set_recv_timeout_ms(client, 2000);

    char req[8192];
    int n = recv_headers(client, req, sizeof(req));
    if (n <= 0) {
      CLOSESOCK(client);
      continue;
    }

    char method[16];
    char path[2048];
    if (parse_request(req, method, sizeof(method), path, sizeof(path)) != 0) {
      const char *msg = "bad request";
      send_simple(client, 400, "Bad Request", "text/plain; charset=utf-8", msg,
                  strlen(msg), 1, NULL, 0);
      CLOSESOCK(client);
      continue;
    }

    int is_get = strcmp(method, "GET") == 0;
    int is_head = strcmp(method, "HEAD") == 0;
    if (!is_get && !is_head) {
      send_simple(client, 405, "Method Not Allowed", "text/plain; charset=utf-8",
                  "", 0, 1, NULL, 0);
      CLOSESOCK(client);
      continue;
    }

    /* The reload channel stays open; everything else is one-shot.
     * (Keep-alive without a timeout starves this single-threaded accept loop
     * when the browser opens parallel connections.) */
    if (strcmp(path, "/@cord/hmr") == 0) {
      if (is_head) {
        send_simple(client, 200, "OK", "text/event-stream", "", 0, 1, NULL, 1);
        CLOSESOCK(client);
      } else {
        sse_add(&sse, client);
      }
      continue;
    }

    DevResponse res;
    memset(&res, 0, sizeof(res));
    int rc = handler(method, path, userdata, &res);
    if (rc != 0) {
      const char *msg = "internal error";
      send_simple(client, 500, "Internal Server Error",
                  "text/plain; charset=utf-8", msg, strlen(msg), 1, NULL,
                  is_head);
      free(res.body);
      CLOSESOCK(client);
      continue;
    }

    size_t len = res.len ? res.len : (res.body ? strlen(res.body) : 0);
    const char *reason = res.status == 404   ? "Not Found"
                         : res.status == 500 ? "Internal Server Error"
                         : res.status == 304 ? "Not Modified"
                                             : "OK";
    send_simple(client, res.status ? res.status : 200, reason, res.content_type,
                res.body, len, res.no_store, res.cache_control, is_head);
    free(res.body);
    CLOSESOCK(client);
  }

  sse_close_all(&sse);
  CLOSESOCK(server);
#ifdef _WIN32
  WSACleanup();
#endif
  printf("\n");
  term_dim("Dev server stopped.");
  return 0;
}
