#ifndef CORDLANG_DEV_SERVER_INTERNAL_H
#define CORDLANG_DEV_SERVER_INTERNAL_H

#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#define CLOSESOCK closesocket
#define SOCK_ERR SOCKET_ERROR
#else
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
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001)
#endif
#endif

#define MAX_SSE 16
typedef struct {
  SOCKET socks[MAX_SSE];
  int count;
} SseSet;

int send_all(SOCKET s, const char *data, size_t len);
int recv_headers(SOCKET s, char *buf, size_t cap);
void set_recv_timeout_ms(SOCKET s, int ms);
void send_simple(SOCKET s, int status, const char *reason, const char *ctype,
                 const char *body, size_t len, int no_store,
                 const char *cache_control, int head_only);
int parse_request(const char *req, char *method, size_t mcap, char *path,
                  size_t pcap);
void sse_add(SseSet *set, SOCKET s);
void sse_broadcast(SseSet *set, const char *event);
void sse_close_all(SseSet *set);

#endif
