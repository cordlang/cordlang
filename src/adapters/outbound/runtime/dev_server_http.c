/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/dev_server_internal.h"
#include <stdio.h>
#include <string.h>
int send_all(SOCKET s, const char *data, size_t len) {
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
int recv_headers(SOCKET s, char *buf, size_t cap) {
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

/* Browsers often open idle TCP sockets (preconnect). Without a recv timeout the
 * single-threaded accept loop blocks forever on those and never serves / again. */
void set_recv_timeout_ms(SOCKET s, int ms) {
#ifdef _WIN32
  DWORD tv = (DWORD)(ms > 0 ? ms : 1);
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
  struct timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
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

void send_simple(SOCKET s, int status, const char *reason,
                        const char *ctype, const char *body, size_t len,
                        int no_store, const char *cache_control,
                        int head_only) {
  char cache_line[160];
  cache_line[0] = '\0';
  if (cache_control && *cache_control)
    snprintf(cache_line, sizeof(cache_line), "Cache-Control: %s\r\n",
             cache_control);
  else if (no_store)
    snprintf(cache_line, sizeof(cache_line), "Cache-Control: no-store\r\n");

  char header[640];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 %d %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "%s"
                      "Connection: close\r\n"
                      "\r\n",
                      status, reason, ctype ? ctype : "text/plain; charset=utf-8",
                      len, cache_line);
  if (hlen > 0) send_all(s, header, (size_t)hlen);
  if (!head_only && body && len) send_all(s, body, len);
}

/* ── request handling ───────────────────────────────────── */

int parse_request(const char *req, char *method, size_t mcap, char *path,
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

