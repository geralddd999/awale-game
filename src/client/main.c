#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "display.h"
#include "net.h"
#include "net_client.h"
#include "protocol.h"
#include "ui.h"
#include "ui_state.h"

static SOCKET init_connection(const char *address, int port) {
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  SOCKADDR_IN sin = {0};
  struct hostent *hostinfo;

  if (sock == INVALID_SOCKET) {
    perror("socket()");
    exit(errno);
  }

  hostinfo = gethostbyname(address);
  if (hostinfo == NULL) {
    fprintf(stderr, "Unknown host %s.\n", address);
    exit(EXIT_FAILURE);
  }

  sin.sin_addr = *(IN_ADDR *)hostinfo->h_addr;
  sin.sin_port = htons(port);
  sin.sin_family = AF_INET;

  if (connect(sock, (SOCKADDR *)&sin, sizeof(SOCKADDR)) == SOCKET_ERROR) {
    perror("connect()");
    exit(errno);
  }
  return sock;
}

static void app_tui(const char *address, int port, const char *name) {
  SOCKET sock = init_connection(address, port);
  LineBuf in;
  char buffer[BUF_SIZE];
  fd_set rdfs;

  srand((unsigned int)time(NULL));
  linebuf_init(&in);
  ui_state_init(name, sock);
  ui_init();

  send_line(sock, "/login %s", name);
  ui_render();

  while (g_ui.running) {
    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);
    FD_SET(sock, &rdfs);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; /* 100ms tick */

    int ret = select(sock + 1, &rdfs, NULL, NULL, &tv);
    if (ret == -1) {
      if (errno == EINTR) {
        g_ui.needs_redraw = 1;
        ui_render();
        continue;
      }
      break;
    }

    ui_tick();

    if (FD_ISSET(STDIN_FILENO, &rdfs)) {
      int ch;
      while ((ch = wgetch(stdscr)) != ERR) {
        ui_handle_key(ch);
      }
    }

    if (FD_ISSET(sock, &rdfs)) {
      if (linebuf_fill(&in, sock) == 0) {
        break;
      }
      while (linebuf_next(&in, buffer, sizeof buffer)) {
        net_client_handle_line(buffer);
      }
    }

    ui_render();
  }

  ui_shutdown();
  closesocket(sock);
}

static int bio_editing = 0;
static int bio_lines = 0;
static char bio_text[BIO_LEN + 1];

static void bio_input(SOCKET sock, const char *input) {
  if (strcmp(input, ".") == 0) {
    send_line(sock, "/bio set %s", bio_text);
    bio_editing = 0;
  } else if (strlen(input) > BIO_LINE_LEN)
    printf("! lines are limited to %d characters, try again\n", BIO_LINE_LEN);
  else if (bio_lines == BIO_LINES)
    printf("! a bio has at most %d lines, type . to finish\n", BIO_LINES);
  else {
    if (bio_lines > 0)
      strcat(bio_text, "\\n");
    strcat(bio_text, input);
    bio_lines++;
  }
}

static void send_input(SOCKET sock, const char *input) {
  if (bio_editing) {
    bio_input(sock, input);
    return;
  }
  if (strcmp(input, "/bio set") == 0) {
    bio_editing = 1;
    bio_lines = 0;
    bio_text[0] = '\0';
    printf("writing your bio: up to %d lines of %d characters, finish with a "
           "line holding only \".\"\n",
           BIO_LINES, BIO_LINE_LEN);
    return;
  }

  if (input[0] == '\0')
    return;
  if (input[0] == '/')
    send_line(sock, "%s", input);
  else if (display_in_game() && input[0] >= '1' && input[0] <= '6' &&
           input[1] == '\0')
    send_line(sock, "/move %s", input);
  else
    send_line(sock, "/say %s", input);
}

static void app_cli(const char *address, int port, const char *name) {
  SOCKET sock = init_connection(address, port);
  LineBuf in;
  char buffer[BUF_SIZE];
  int stdin_open = 1;
  fd_set rdfs;

  linebuf_init(&in);
  display_init(name);
  send_line(sock, "/login %s", name);

  while (1) {
    FD_ZERO(&rdfs);
    if (stdin_open)
      FD_SET(STDIN_FILENO, &rdfs);
    FD_SET(sock, &rdfs);

    if (select(sock + 1, &rdfs, NULL, NULL, NULL) == -1) {
      perror("select()");
      exit(errno);
    }

    if (stdin_open && FD_ISSET(STDIN_FILENO, &rdfs)) {
      if (fgets(buffer, sizeof buffer, stdin) == NULL) {
        send_line(sock, "/quit");
        stdin_open = 0;
      } else {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        send_input(sock, buffer);
      }
    }

    if (FD_ISSET(sock, &rdfs)) {
      if (linebuf_fill(&in, sock) == 0) {
        printf("Server disconnected !\n");
        break;
      }
      while (linebuf_next(&in, buffer, sizeof buffer))
        display_line(buffer);
    }
  }

  closesocket(sock);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage : %s [address] [username] [port]\n", argv[0]);
    return EXIT_FAILURE;
  }

  int port = (argc > 3) ? atoi(argv[3]) : PORT;
  if (isatty(STDIN_FILENO))
    app_tui(argv[1], port, argv[2]);
  else
    app_cli(argv[1], port, argv[2]);

  return EXIT_SUCCESS;
}
