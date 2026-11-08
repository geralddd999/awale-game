#ifndef NET_H
#define NET_H

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define PORT 1977
#define BUF_SIZE 1024

/*
 * Bytes received from a socket that do not form a complete line yet.
 * A message is one line terminated by '\n' (a '\r' before it is dropped).
 */
typedef struct {
  char data[BUF_SIZE];
  int len;
} LineBuf;

void linebuf_init(LineBuf *lb);
/* append raw bytes, returns -1 if they do not fit */
int linebuf_push(LineBuf *lb, const char *bytes, size_t n);
/* one recv() into the buffer: >0 bytes read, 0 on EOF or error */
int linebuf_fill(LineBuf *lb, SOCKET sock);
/* pop the next complete line into out (without the line ending), returns 1 if a
 * line was popped */
int linebuf_next(LineBuf *lb, char *out, size_t size);
/* buffer is full and still holds no complete line: the peer sent a line that is
 * too long */
int linebuf_overflow(const LineBuf *lb);

/* format a message and send it with a trailing '\n', returns -1 on failure */
int send_line(SOCKET sock, const char *fmt, ...);
int send_linev(SOCKET sock, const char *fmt, va_list args);

#endif
