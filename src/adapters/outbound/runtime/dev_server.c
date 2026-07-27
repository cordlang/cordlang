/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/dev_server.h"
#include "adapters/outbound/process/process_spawn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
typedef int socklen_t;
#define CLOSESOCK closesocket
#define SOCK_ERR SOCKET_ERROR
#else
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#define SOCK_ERR (-1)
#endif

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

static int send_all(SOCKET s, const char *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
#ifdef _WIN32
    int n = send(s, data + sent, (int)(len - sent), 0);
#else
    ssize_t n = send(s, data + sent, len - sent, 0);
#endif
    if (n <= 0) return -1;
    sent += (size_t)n;
  }
  return 0;
}

/* Read request headers up to the blank line. Returns bytes read, <=0 on error. */
static int recv_headers(SOCKET s, char *buf, size_t cap) {
  size_t total = 0;
  while (total + 1 < cap) {
#ifdef _WIN32
    int n = recv(s, buf + total, (int)(cap - total - 1), 0);
#else
    ssize_t n = recv(s, buf + total, cap - total - 1, 0);
#endif
    if (n <= 0) break;
    total += (size_t)n;
    buf[total] = '\0';
    if (strstr(buf, "\r\n\r\n") || strstr(buf, "\n\n")) break;
  }
  if (total == 0) return 0;
  buf[total] = '\0';
  return (int)total;
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void percent_decode(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = in; *p && o + 1 < cap; p++) {
    if (*p == '%' && p[1] && p[2]) {
      int hi = hexval(p[1]), lo = hexval(p[2]);
      if (hi >= 0 && lo >= 0) {
        out[o++] = (char)((hi << 4) | lo);
        p += 2;
        continue;
      }
    }
    out[o++] = *p;
  }
  out[o] = '\0';
}

static void send_simple(SOCKET s, int status, const char *reason,
                        const char *ctype, const char *body, size_t len,
                        int no_store, int head_only) {
  char header[512];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "%s"
                      "Connection: close\r\n"
                      "\r\n",
                      status, reason, ctype ? ctype : "text/plain; charset=utf-8",
                      len, no_store ? "Cache-Control: no-store\r\n" : "");
  if (hlen > 0) send_all(s, header, (size_t)hlen);
  if (!head_only && body && len) send_all(s, body, len);
}

/* ── SSE reload channel ─────────────────────────────────── */

#define MAX_SSE 16

typedef struct {
  SOCKET socks[MAX_SSE];
  int count;
} SseSet;

static void sse_add(SseSet *set, SOCKET s) {
  const char *head =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/event-stream\r\n"
      "Cache-Control: no-store\r\n"
      "Connection: keep-alive\r\n"
      "X-Accel-Buffering: no\r\n"
      "\r\n"
      "retry: 500\n\n";
  if (send_all(s, head, strlen(head)) != 0) {
    CLOSESOCK(s);
    return;
  }
  if (set->count >= MAX_SSE) {
    /* Drop the oldest listener rather than refusing the new tab. */
    CLOSESOCK(set->socks[0]);
    memmove(set->socks, set->socks + 1, sizeof(SOCKET) * (MAX_SSE - 1));
    set->count--;
  }
  set->socks[set->count++] = s;
}

static void sse_broadcast(SseSet *set, const char *event) {
  char msg[640];
  int n = snprintf(msg, sizeof(msg), "data: %s\n\n", event ? event : "reload");
  if (n <= 0 || (size_t)n >= sizeof(msg)) return;
  for (int i = 0; i < set->count;) {
    if (send_all(set->socks[i], msg, (size_t)n) != 0) {
      CLOSESOCK(set->socks[i]);
      memmove(set->socks + i, set->socks + i + 1,
              sizeof(SOCKET) * (size_t)(set->count - i - 1));
      set->count--;
      continue;
    }
    i++;
  }
}

static void sse_close_all(SseSet *set) {
  for (int i = 0; i < set->count; i++) CLOSESOCK(set->socks[i]);
  set->count = 0;
}

/* ── change detection ───────────────────────────────────── */

/*
 * Fingerprint of every .cord under the project, every file under public/, and
 * cordlang.json. A file list lets soft HMR know whether a single leaf .cord
 * changed (update) vs entry / public / config (full reload).
 */
typedef struct {
  unsigned long long hash;
  int files;
} WatchStamp;

#define MAX_WATCH_FILES 512

typedef struct {
  char rel[512]; /* project-relative, / separators, e.g. src/app.cord */
  unsigned long long sig;
} WatchFile;

typedef struct {
  WatchStamp stamp;
  WatchFile files[MAX_WATCH_FILES];
  int n_files;
} WatchSnapshot;

static void path_to_rel(const char *root, const char *abs, char *out, size_t n) {
  size_t rl = root ? strlen(root) : 0;
  const char *p = abs;
  if (root && strncmp(abs, root, rl) == 0) {
    p = abs + rl;
    while (*p == '/' || *p == '\\') p++;
  }
  size_t i = 0;
  for (; p[i] && i + 1 < n; i++)
    out[i] = (p[i] == '\\') ? '/' : p[i];
  out[i] = '\0';
}

static unsigned long long file_sig(const char *path, time_t mtime, long long size) {
  unsigned long long h = 14695981039346656037ULL;
  for (const char *p = path; *p; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == '\\') c = '/';
    h = h * 1099511628211ULL ^ (unsigned long long)(unsigned char)c;
  }
  h = h * 1099511628211ULL ^ (unsigned long long)mtime;
  h = h * 1099511628211ULL ^ (unsigned long long)size;
  return h;
}

static void stamp_add(WatchSnapshot *snap, const char *root, const char *abs,
                      time_t mtime, long long size) {
  unsigned long long sig = file_sig(abs, mtime, size);
  snap->stamp.hash ^= sig + (unsigned long long)snap->stamp.files * 0x9e3779b97f4a7c15ULL;
  snap->stamp.files++;
  if (snap->n_files < MAX_WATCH_FILES) {
    WatchFile *f = &snap->files[snap->n_files++];
    path_to_rel(root, abs, f->rel, sizeof(f->rel));
    f->sig = sig;
  }
}

static int is_cord_file(const char *name) {
  size_t n = strlen(name);
  if (n < 6) return 0;
  return strcmp(name + n - 5, ".cord") == 0 || strcmp(name + n - 5, ".CORD") == 0;
}

static int skip_dir(const char *name) {
  return strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
         strcmp(name, "node_modules") == 0 || strcmp(name, ".git") == 0 ||
         strcmp(name, "dist") == 0;
}

static int basename_is(const char *path, const char *name) {
  const char *base = path;
  for (const char *p = path; *p; p++)
    if (*p == '/' || *p == '\\') base = p + 1;
  return strcmp(base, name) == 0;
}

#ifdef _WIN32
static void walk_stamp(const char *root, const char *dir, WatchSnapshot *snap,
                       int depth, int in_public) {
  if (depth > 16) return;
  char pattern[4096];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (skip_dir(fd.cFileName)) continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
    int next_pub = in_public || basename_is(path, "public");
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      walk_stamp(root, path, snap, depth + 1, next_pub);
    } else if (is_cord_file(fd.cFileName) || in_public || next_pub) {
      /* Files directly in public/ are stamped when parent is public. */
      if (!(is_cord_file(fd.cFileName) || in_public)) continue;
      ULARGE_INTEGER t;
      t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
      t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
      stamp_add(snap, root, path, (time_t)(t.QuadPart / 10000000ULL),
                (long long)fd.nFileSizeLow);
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}
#else
static void walk_stamp(const char *root, const char *dir, WatchSnapshot *snap,
                       int depth, int in_public) {
  if (depth > 16) return;
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (skip_dir(ent->d_name)) continue;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat sb;
    if (stat(path, &sb) != 0) continue;
    int next_pub = in_public || basename_is(path, "public");
    if (S_ISDIR(sb.st_mode)) {
      walk_stamp(root, path, snap, depth + 1, next_pub);
    } else if (is_cord_file(ent->d_name) || in_public) {
      stamp_add(snap, root, path, sb.st_mtime, (long long)sb.st_size);
    }
  }
  closedir(d);
}
#endif

static WatchSnapshot take_snapshot(const char *root) {
  WatchSnapshot snap;
  memset(&snap, 0, sizeof(snap));
  snap.stamp.hash = 14695981039346656037ULL;
  if (!root) return snap;
  walk_stamp(root, root, &snap, 0, 0);
  char cfg[4096];
  snprintf(cfg, sizeof(cfg), "%s/cordlang.json", root);
  struct stat sb;
  if (stat(cfg, &sb) == 0)
    stamp_add(&snap, root, cfg, sb.st_mtime, (long long)sb.st_size);
  return snap;
}

static WatchStamp take_stamp(const char *root) {
  return take_snapshot(root).stamp;
}

unsigned long long dev_server_project_stamp(const char *watch_dir) {
  return take_stamp(watch_dir).hash;
}

static int stamp_equal(const WatchStamp *a, const WatchStamp *b) {
  return a->hash == b->hash && a->files == b->files;
}

/*
 * Compare snapshots. Sets *out_url to "/rel" for a single soft-updatable .cord
 * change, or leaves it empty for full reload. Returns 1 if full_reload.
 */
static int classify_change(const WatchSnapshot *prev, const WatchSnapshot *now,
                           const char *entry_rel, char *out_url, size_t out_n) {
  if (out_url && out_n) out_url[0] = '\0';
  int changed = 0;
  char only[512];
  only[0] = '\0';

  for (int i = 0; i < now->n_files; i++) {
    const WatchFile *nf = &now->files[i];
    int found = 0;
    unsigned long long old_sig = 0;
    for (int j = 0; j < prev->n_files; j++) {
      if (strcmp(prev->files[j].rel, nf->rel) == 0) {
        found = 1;
        old_sig = prev->files[j].sig;
        break;
      }
    }
    if (!found || old_sig != nf->sig) {
      changed++;
      if (changed == 1) snprintf(only, sizeof(only), "%s", nf->rel);
      else only[0] = '\0';
    }
  }
  for (int j = 0; j < prev->n_files; j++) {
    int found = 0;
    for (int i = 0; i < now->n_files; i++) {
      if (strcmp(now->files[i].rel, prev->files[j].rel) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      changed++;
      only[0] = '\0';
    }
  }

  if (changed != 1 || !only[0]) return 1;

  /* public/ or cordlang.json → full reload */
  if (strncmp(only, "public/", 7) == 0 || strcmp(only, "cordlang.json") == 0)
    return 1;
  size_t n = strlen(only);
  if (n < 6 || strcmp(only + n - 5, ".cord") != 0) return 1;

  /* entry file → full reload (routes/theme) */
  if (entry_rel && *entry_rel) {
    char er[512];
    size_t i = 0;
    const char *e = entry_rel;
    while (*e == '/') e++;
    for (; e[i] && i + 1 < sizeof(er); i++)
      er[i] = (e[i] == '\\') ? '/' : e[i];
    er[i] = '\0';
    if (strcmp(only, er) == 0) return 1;
  }

  if (out_url && out_n) snprintf(out_url, out_n, "/%s", only);
  return 0;
}

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

/* ── request handling ───────────────────────────────────── */

static int parse_request(const char *req, char *method, size_t mcap, char *path,
                         size_t pcap) {
  const char *sp1 = strchr(req, ' ');
  if (!sp1) return -1;
  size_t mlen = (size_t)(sp1 - req);
  if (mlen == 0 || mlen + 1 > mcap) return -1;
  memcpy(method, req, mlen);
  method[mlen] = '\0';

  const char *target = sp1 + 1;
  const char *sp2 = strpbrk(target, " \r\n");
  if (!sp2) return -1;
  size_t tlen = (size_t)(sp2 - target);
  char raw[2048];
  if (tlen + 1 > sizeof(raw)) tlen = sizeof(raw) - 1;
  memcpy(raw, target, tlen);
  raw[tlen] = '\0';

  /* strip query and fragment */
  char *q = strpbrk(raw, "?#");
  if (q) *q = '\0';

  percent_decode(raw, path, pcap);
  if (!path[0]) snprintf(path, pcap, "/");
  return 0;
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

  printf("\n");
  printf("  Cordlang dev server (modulos ES nativos)\n");
  printf("  ----------------------------------------\n");
  printf("  Local:   %s\n", url);
  printf("  Entry:   %s\n", entry_label ? entry_label : "src/app.cord");
  printf("  Sirve:   .cord compilado por peticion, sin bundler ni Node\n");
  if (watch_dir)
    printf("  Watch:   .cord / public / cordlang.json (soft update o reload)\n");
  printf("  Stop:    Ctrl+C\n");
  printf("\n");
  fflush(stdout);

  if (open_browser) dev_server_open_browser(url);

  SseSet sse;
  memset(&sse, 0, sizeof(sse));
  for (int i = 0; i < MAX_SSE; i++) sse.socks[i] = INVALID_SOCKET;
  sse.count = 0;

  WatchSnapshot snap = take_snapshot(watch_dir);
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
        WatchSnapshot now = take_snapshot(watch_dir);
        if (!stamp_equal(&snap.stamp, &now.stamp)) {
          char changed_url[560];
          int full = classify_change(&snap, &now, entry_label, changed_url,
                                     sizeof(changed_url));
          snap = now;
          if (on_change)
            on_change(userdata, full ? NULL : changed_url, full);
          if (full) {
            printf("cambio detectado -> reload\n");
            fflush(stdout);
            sse_broadcast(&sse, "reload");
          } else {
            char ev[600];
            snprintf(ev, sizeof(ev), "update:%s", changed_url);
            printf("cambio detectado -> soft update %s\n", changed_url);
            fflush(stdout);
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
                  strlen(msg), 1, 0);
      CLOSESOCK(client);
      continue;
    }

    int is_get = strcmp(method, "GET") == 0;
    int is_head = strcmp(method, "HEAD") == 0;
    if (!is_get && !is_head) {
      send_simple(client, 405, "Method Not Allowed", "text/plain; charset=utf-8",
                  "", 0, 1, 0);
      CLOSESOCK(client);
      continue;
    }

    /* The reload channel stays open; everything else is one-shot. */
    if (strcmp(path, "/@cord/hmr") == 0) {
      if (is_head) {
        send_simple(client, 200, "OK", "text/event-stream", "", 0, 1, 1);
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
                  "text/plain; charset=utf-8", msg, strlen(msg), 1, is_head);
      free(res.body);
      CLOSESOCK(client);
      continue;
    }

    size_t len = res.len ? res.len : (res.body ? strlen(res.body) : 0);
    const char *reason = res.status == 404 ? "Not Found"
                         : res.status == 500 ? "Internal Server Error"
                         : res.status == 304 ? "Not Modified"
                                             : "OK";
    send_simple(client, res.status ? res.status : 200, reason, res.content_type,
                res.body, len, res.no_store, is_head);
    free(res.body);
    CLOSESOCK(client);
  }

  sse_close_all(&sse);
  CLOSESOCK(server);
#ifdef _WIN32
  WSACleanup();
#endif
  printf("\nDev server detenido.\n");
  return 0;
}
