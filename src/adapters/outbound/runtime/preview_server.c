/* Feature-test macros must precede system headers (glibc / POSIX). */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "adapters/outbound/runtime/preview_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
/* Linux/macOS: sockets + select(2) for interruptible accept loop */
#include <unistd.h>
#include <errno.h>
#include <signal.h>
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

#ifndef _WIN32
static void on_sigint(int sig) {
  (void)sig;
  g_running = 0;
}
#endif

#ifdef _WIN32
static BOOL WINAPI on_console_ctrl(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
    g_running = 0;
    return TRUE;
  }
  return FALSE;
}
#endif

void preview_open_browser(const char *url) {
  if (!url) return;
#ifdef _WIN32
  ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1 &", url);
  system(cmd);
#else
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 || sensible-browser '%s' >/dev/null 2>&1 &",
           url, url);
  system(cmd);
#endif
}

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

static void handle_client(SOCKET client, const char *html, size_t html_len) {
  char req[2048];
  int n;
#ifdef _WIN32
  n = recv(client, req, (int)sizeof(req) - 1, 0);
#else
  n = (int)recv(client, req, sizeof(req) - 1, 0);
#endif
  if (n <= 0) {
    CLOSESOCK(client);
    return;
  }
  req[n] = '\0';

  /* Very small HTTP: only care about method/path start */
  int is_get = (strncmp(req, "GET ", 4) == 0);
  int is_head = (strncmp(req, "HEAD ", 5) == 0);

  if (!is_get && !is_head) {
    const char *resp =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    send_all(client, resp, strlen(resp));
    CLOSESOCK(client);
    return;
  }

  /* Serve index for any path (SPA-style preview) */
  char header[512];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html; charset=utf-8\r\n"
                      "Content-Length: %zu\r\n"
                      "Cache-Control: no-store\r\n"
                      "Connection: close\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "\r\n",
                      html_len);
  if (hlen > 0) send_all(client, header, (size_t)hlen);
  if (is_get) send_all(client, html, html_len);
  CLOSESOCK(client);
}

int preview_server_serve(const char *html, size_t html_len, int port) {
  if (!html) return 1;
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
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.0.0.1 only */

  if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_ERR) {
    /* try next ports if busy */
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
      fprintf(stderr, "Error: cannot bind preview server (ports %d-%d busy)\n", port, port + 19);
      CLOSESOCK(server);
#ifdef _WIN32
      WSACleanup();
#endif
      return 1;
    }
  }

  if (listen(server, 8) == SOCK_ERR) {
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
  printf("  Cordlang Runtime Preview\n");
  printf("  ------------------------\n");
  printf("  Local:   %s\n", url);
  printf("  Serving: native HTML runtime (no Node / no React)\n");
  printf("  Stop:    Ctrl+C\n");
  printf("\n");
  fflush(stdout);

  preview_open_browser(url);

  g_running = 1;
  while (g_running) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(server, &rfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 250000; /* 250ms so Ctrl+C is responsive */

    int sel = select((int)server + 1, &rfds, NULL, NULL, &tv);
    if (sel == SOCK_ERR) {
#ifdef _WIN32
      if (!g_running) break;
#else
      if (errno == EINTR) continue;
      if (!g_running) break;
#endif
      break;
    }
    if (sel == 0) continue;
    if (!FD_ISSET(server, &rfds)) continue;

    struct sockaddr_in client_addr;
    socklen_t clen = sizeof(client_addr);
    SOCKET client = accept(server, (struct sockaddr *)&client_addr, &clen);
    if (client == INVALID_SOCKET) continue;
    handle_client(client, html, html_len);
  }

  CLOSESOCK(server);
#ifdef _WIN32
  WSACleanup();
#endif
  printf("\nPreview stopped.\n");
  return 0;
}
