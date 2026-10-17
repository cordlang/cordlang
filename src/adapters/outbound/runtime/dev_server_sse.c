/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/dev_server_internal.h"
#include <stdio.h>
#include <string.h>
/* ── SSE reload channel ─────────────────────────────────── */

void sse_add(SseSet *set, SOCKET s) {
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

void sse_broadcast(SseSet *set, const char *event) {
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

void sse_close_all(SseSet *set) {
  for (int i = 0; i < set->count; i++) CLOSESOCK(set->socks[i]);
  set->count = 0;
}

