#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "commands.h"
#include "friends.h"
#include "history.h"
#include "lobby.h"
#include "session.h"

#define DATA_DIR "data"
#define GAMES_DIR DATA_DIR "/games"
/* a client that stops reading can block send() for at most this long before
 * being dropped */
#define SEND_TIMEOUT_SEC 2

static SOCKET init_connection(int port) {
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  SOCKADDR_IN sin = {0};
  int yes = 1;

  if (sock == INVALID_SOCKET) {
    perror("socket()");
    exit(errno);
  }
  /* restart the server right away without waiting for TIME_WAIT */
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port);
  sin.sin_family = AF_INET;

  if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR) {
    perror("bind()");
    exit(errno);
  }
  if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR) {
    perror("listen()");
    exit(errno);
  }
  return sock;
}

static void accept_client(SOCKET listener) {
  SOCKADDR_IN csin = {0};
  socklen_t sinsize = sizeof csin;
  SOCKET csock = accept(listener, (SOCKADDR *)&csin, &sinsize);
  if (csock == SOCKET_ERROR) {
    perror("accept()");
    return;
  }

  struct timeval timeout = {SEND_TIMEOUT_SEC, 0};
  setsockopt(csock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

  /* nothing is read here: the client logs in with a /login line like any other
   * command */
  if (session_alloc(csock) < 0) {
    send_line(csock, "ERR SERVER_FULL too many clients");
    closesocket(csock);
  }
}

static void read_client(int slot) {
  Client *c = &clients[slot];
  char line[BUF_SIZE];

  if (linebuf_fill(&c->in, c->sock) == 0) {
    c->dead = 1;
    return;
  }
  /* run every complete line, keep a partial one for the next recv() */
  while (!c->dead && linebuf_next(&c->in, line, sizeof line))
    handle_line(slot, line);

  if (linebuf_overflow(&c->in)) {
    session_send(slot, "ERR LINE_TOO_LONG lines are limited to %d bytes",
                 BUF_SIZE - 1);
    c->dead = 1;
  }
}

/* closing a client can make sends to others fail, so repeat until nobody is
 * left to reap */
static void reap_dead(void) {
  int again = 1;
  while (again) {
    again = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].state == ST_FREE || !clients[i].dead)
        continue;

      lobby_leave(i, "disconnect");
      if (session_logged(i))
        session_broadcast_hall(i, "EVT USER_LEFT %s", clients[i].name);
      session_free(i);
      again = 1;
    }
  }
}

static void app(int port) {
  SOCKET listener = init_connection(port);
  fd_set rdfs;
  struct timeval tv;
  printf("Awale server listening on port %d\n", port);

  while (1) {
    int max = listener;
    FD_ZERO(&rdfs);
    FD_SET(listener, &rdfs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].state == ST_FREE)
        continue;
      FD_SET(clients[i].sock, &rdfs);
      if (clients[i].sock > max)
        max = clients[i].sock;
    }

    /* wake up on our own when the next invitation expires, otherwise wait for
     * the sockets only */
    int wait = lobby_next_expiry(time(NULL));
    tv.tv_sec = wait;
    tv.tv_usec = 0;

    if (select(max + 1, &rdfs, NULL, NULL, wait >= 0 ? &tv : NULL) == -1) {
      if (errno == EINTR)
        continue;
      perror("select()");
      exit(errno);
    }

    if (FD_ISSET(listener, &rdfs))
      accept_client(listener);

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].state != ST_FREE && !clients[i].dead &&
          FD_ISSET(clients[i].sock, &rdfs))
        read_client(i);
    }
    lobby_expire_invites(time(NULL));
    reap_dead();
  }
}

int main(int argc, char **argv) {
  int port = (argc > 1) ? atoi(argv[1]) : PORT;
  /* optional invitation delay in seconds, mostly to keep the tests short */
  if (argc > 2)
    lobby_set_invite_timeout(atoi(argv[2]));

  /* a write to a closed connection must not kill the server */
  signal(SIGPIPE, SIG_IGN);
  srand(time(NULL));

  mkdir(DATA_DIR, 0755);
  mkdir(GAMES_DIR, 0755);
  users_load(DATA_DIR "/users.txt");
  friends_load(DATA_DIR "/friends.txt");
  history_init(GAMES_DIR);
  session_init();
  lobby_init();

  app(port);
  return EXIT_SUCCESS;
}
