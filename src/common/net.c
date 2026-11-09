#include "net.h"
#include <stdio.h>
#include <string.h>

void linebuf_init(LineBuf *lb) { lb->len = 0; }

// alocate data from another buffer to the linebuffer
// used for tests purpose
int linebuf_push(LineBuf *lb, const char *bytes, size_t n) {
  if (n > (size_t)(BUF_SIZE - lb->len))
    return -1;
  memcpy(lb->data + lb->len, bytes, n);
  lb->len += n;
  return 0;
}

// alocate data from the socket to the linebuffer
int linebuf_fill(LineBuf *lb, SOCKET sock) {
  if (lb->len >= BUF_SIZE)
    return 0;

  int n = recv(sock, lb->data + lb->len, BUF_SIZE - lb->len, 0);
  if (n <= 0)
    return 0;

  lb->len += n;
  return n;
}

// function responsible for finding complete messages (ending in '\n')
// accumulated in the LineBuffer
int linebuf_next(LineBuf *lb, char *out, size_t size) {
  // pointer to the memory region where is a char '\n'
  char *nl = memchr(lb->data, '\n', lb->len);
  // message still not complete
  if (nl == NULL)
    return 0;

  // size of the message
  size_t line_len = nl - lb->data;
  size_t copy = line_len;
  if (copy > 0 && lb->data[copy - 1] == '\r')
    copy--;
  if (copy >= size)
    copy = size - 1;

  // copies message to out
  memcpy(out, lb->data, copy);
  out[copy] = '\0';

  // keep the bytes that follow the line for the next call
  lb->len -= line_len + 1;
  memmove(lb->data, nl + 1, lb->len);
  return 1;
}

// checks for linebuffer overflow
int linebuf_overflow(const LineBuf *lb) {
  return lb->len >= BUF_SIZE && memchr(lb->data, '\n', lb->len) == NULL;
}

// formats and sends a message through a socket
int send_linev(SOCKET sock, const char *fmt, va_list args) {
  char msg[BUF_SIZE];
  int len = vsnprintf(msg, sizeof msg - 1, fmt, args);
  if (len < 0)
    return -1;
  if (len > (int)sizeof msg - 2)
    len = sizeof msg - 2;
  msg[len++] = '\n';

  // send() may write only part of the buffer, loop until all of it is out
  int sent = 0;
  while (sent < len) {
    int n = send(sock, msg + sent, len - sent, MSG_NOSIGNAL);
    if (n <= 0)
      return -1;
    sent += n;
  }
  return 0;
}

int send_line(SOCKET sock, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int res = send_linev(sock, fmt, args);
  va_end(args);
  return res;
}
