#include "display.h"
#include "board_view.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>

static char me[NAME_LEN + 1];
static char players[2][NAME_LEN + 1];
static int my_side = -1; /* 0 or 1 while seated in a game, -1 otherwise */
static int playing = 0;

void display_init(const char *name) { snprintf(me, sizeof me, "%s", name); }

int display_in_game(void) { return playing && my_side >= 0; }

static int starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void show_board(const char *line) {
  Board b;
  char lobby[32], out[BOARD_VIEW_MAX];
  int turn, winner;
  int n =
      sscanf(line, "BOARD %31s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             lobby, &turn, &winner, &b.houses[0], &b.houses[1], &b.houses[2],
             &b.houses[3], &b.houses[4], &b.houses[5], &b.houses[6],
             &b.houses[7], &b.houses[8], &b.houses[9], &b.houses[10],
             &b.houses[11], &b.captured[0], &b.captured[1]);
  if (n != 17) {
    puts(line);
    return;
  }

  b.turn = (Side)turn;
  b.winner = (Winner)winner;
  b.status = (winner == WINNER_NONE) ? GAME_ONGOING : GAME_FINISHED;
  const char *names[2] = {players[0], players[1]};

  render_board(&b, my_side == PLAYER_1 ? PLAYER_1 : PLAYER_0, names, out,
               sizeof out);
  printf("\n%s", out);
  if (b.status == GAME_ONGOING && lobby[0] != '#' && turn == my_side)
    puts(">> your move: type 1-6");
}

void display_line(const char *line) {
  char a[64], b[64], c[64], d[64];
  int n;

  if (starts_with(line, "BOARD "))
    show_board(line);
  else if (starts_with(line, "ERR ")) {
    /* skip the code, the text is meant for humans */
    const char *text = strchr(line + 4, ' ');
    printf("! %s\n", text ? text + 1 : line + 4);
  } else if (sscanf(line, "EVT START %63s %16s %16s %63s %63s", a, players[0],
                    players[1], b, c) == 5) {
    my_side = (strcmp(b, "spectator") == 0) ? -1 : b[0] - '0';
    playing = 1;
    printf("== game starts in %s's lobby: %s vs %s%s\n", a, players[0],
           players[1], my_side < 0 ? " (spectating)" : "");
    printf("== the server drew %s to begin\n", strcmp(c, me) == 0 ? "you" : c);
  } else if (sscanf(line, "EVT END %63s %63s %63s %63s %n", a, b, c, d, &n) ==
             4) {
    playing = 0;
    const char *reason = line + n;
    if (strcmp(b, "draw") == 0)
      printf("== draw %s-%s", c, d);
    else
      printf("== %s wins %s-%s", b, c, d);
    printf(" (%.*s)\n", (int)strcspn(reason, " "), reason);
  } else if (sscanf(line, "OK replay %63s %63s %63s %16s %16s", a, b, c,
                    players[0], players[1]) == 5) {
    my_side = -1;
    printf("== replay of game %s, move %s/%s\n", a, b, c);
  } else if (sscanf(line, "EVT CHAT %63s %n", a, &n) == 1)
    printf("[%s] %s\n", a, line + n);
  else if (sscanf(line, "EVT PM %63s %n", a, &n) == 1)
    printf("(private) %s: %s\n", a, line + n);
  /* must come before "EVT INVITE %s", which would also match these lines */
  else if (sscanf(line, "EVT INVITE_%15s %63s %63s", a, b, c) == 3) {
    const char *what = strcmp(a, "DENIED") == 0    ? "was denied"
                       : strcmp(a, "EXPIRED") == 0 ? "expired"
                                                   : "was cancelled";
    if (strcmp(c, me) == 0)
      printf("== the invitation to %s's lobby %s\n", b, what);
    else
      printf("== your invitation to %s %s\n", c, what);
  } else if (sscanf(line, "EVT INVITE %63s %63s", a, b) == 2)
    printf(
        "== %s invites you to their lobby: /accept or /deny (expires in %ss)\n",
        a, b);
  else if (sscanf(line, "EVT MOVE %63s %63s %63s", a, b, c) == 3)
    printf("== %s played house %s\n", b, c);
  else if (sscanf(line, "EVT FRIEND_REQ %63s", a) == 1)
    printf("== %s sent you a friend request (/friend %s to accept, /unfriend "
           "%s to decline)\n",
           a, a, a);
  else if (sscanf(line, "EVT FRIEND_ACCEPT %63s", a) == 1)
    printf("== %s accepted your friend request!\n", a);
  else if (sscanf(line, "EVT UNFRIEND %63s", a) == 1)
    printf("== %s removed you from their friends/requests\n", a);
  else if (sscanf(line, "OK bio %63s %d", a, &n) == 2)
    printf("== bio of %s:%s\n", a, n == 0 ? " (empty)" : "");
  else if (starts_with(line, "BIO "))
    printf("   %s\n", line + 4);
  else if (starts_with(line, "OK say") || starts_with(line, "OK msg") ||
           starts_with(line, "OK move"))
    return;
  else if (starts_with(line, "OK "))
    printf("ok: %s\n", line + 3);
  else if (starts_with(line, "EVT "))
    printf("== %s\n", line + 4);
  else
    puts(line);

  fflush(stdout);
}
