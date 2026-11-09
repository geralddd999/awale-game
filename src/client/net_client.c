#include "net_client.h"
#include "ui_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pending_recreate = 0; /* 0 = none, 1 = public, 2 = private */
static int bio_from_chat = 0;
static int bio_is_self = 0;
static int bio_chat_lines_left = 0;

static int starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

void net_client_notify_chat_cmd(const char *cmd) {
  if (starts_with(cmd, "/bio") && !starts_with(cmd, "/bio set")) {
    bio_from_chat = 1;
  } else if (starts_with(cmd, "/msg ")) {
    char target[NAME_LEN + 1];
    int n = 0;
    if (sscanf(cmd, "/msg %16s %n", target, &n) == 1 && n > 0 &&
        cmd[n] != '\0') {
      char label[NAME_LEN + 8];
      snprintf(label, sizeof(label), "to %s", target);
      ui_state_add_chat(CHAT_PM, label, cmd + n);
    }
  }
}

void net_client_toggle_lobby_privacy(void) {
  if (!g_ui.in_lobby || !g_ui.is_leader || g_ui.playing)
    return;
  pending_recreate = g_ui.is_private ? 1 : 2;
  send_line(g_ui.sock, "/leave");
}

static void parse_board_line(const char *line) {
  Board b;
  char target[32];
  int turn, winner;
  int n =
      sscanf(line, "BOARD %31s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
             target, &turn, &winner, &b.houses[0], &b.houses[1], &b.houses[2],
             &b.houses[3], &b.houses[4], &b.houses[5], &b.houses[6],
             &b.houses[7], &b.houses[8], &b.houses[9], &b.houses[10],
             &b.houses[11], &b.captured[0], &b.captured[1]);
  if (n != 17)
    return;

  b.turn = (Side)turn;
  b.winner = (Winner)winner;
  b.status = (winner == WINNER_NONE) ? GAME_ONGOING : GAME_FINISHED;
  b.idle_moves = 0;

  if (target[0] == '#') {
    g_ui.replay_board = b;
  } else {
    g_ui.online_board = b;
  }
  g_ui.needs_redraw = 1;
}

static void remove_spectator(const char *name) {
  for (int i = 0; i < g_ui.num_spectators; i++) {
    if (strcmp(g_ui.lobby_spectators[i], name) == 0) {
      memmove(&g_ui.lobby_spectators[i], &g_ui.lobby_spectators[i + 1],
              (g_ui.num_spectators - i - 1) * sizeof(g_ui.lobby_spectators[0]));
      g_ui.num_spectators--;
      return;
    }
  }
}

static void add_spectator(const char *name) {
  if (g_ui.num_spectators >= MAX_LOBBY_MEMBERS)
    return;
  for (int i = 0; i < g_ui.num_spectators; i++) {
    if (strcmp(g_ui.lobby_spectators[i], name) == 0)
      return;
  }
  snprintf(g_ui.lobby_spectators[g_ui.num_spectators++], NAME_LEN + 1, "%s",
           name);
}

void net_client_handle_line(const char *line) {
  char a[NAME_LEN + 1], b[NAME_LEN + 1], c[NAME_LEN + 1], d[NAME_LEN + 1],
      e[NAME_LEN + 1];
  char msg[160];
  int n, v1, v2, v3, v4;

  g_ui.needs_redraw = 1;

  if (starts_with(line, "BOARD ")) {
    parse_board_line(line);
    return;
  }

  if (starts_with(line, "ERR ")) {
    const char *text = strchr(line + 4, ' ');
    const char *msg = text ? text + 1 : line + 4;
    ui_state_set_status(msg, 1);
    ui_state_add_chat(CHAT_ERROR, "Error", msg);
    return;
  }

  /* Multi-line list items */
  if (sscanf(line, "USER %16s %15s %16s", a, b, c) >= 2) {
    if (g_ui.num_online < MAX_CLIENTS) {
      OnlineUser *u = &g_ui.online_users[g_ui.num_online++];
      snprintf(u->name, sizeof(u->name), "%s", a);
      u->in_lobby = (strcmp(b, "lobby") == 0);
      snprintf(u->lobby, sizeof(u->lobby), "%s", u->in_lobby ? c : "");
    }
    if (g_ui.in_lobby && strcmp(b, "lobby") == 0 &&
        strcmp(c, g_ui.lobby_leader) == 0) {
      if (strcmp(a, g_ui.lobby_leader) != 0 &&
          strcmp(a, g_ui.lobby_opponent) != 0) {
        add_spectator(a);
      }
    }
    return;
  }

  if (sscanf(line, "LOBBY %16s %15s %15s %16s %d", a, b, c, d, &v1) == 5) {
    if (g_ui.num_lobbies < MAX_LOBBIES) {
      LobbyInfo *l = &g_ui.lobbies[g_ui.num_lobbies++];
      snprintf(l->leader, sizeof(l->leader), "%s", a);
      l->playing = (strcmp(b, "playing") == 0);
      l->is_private = (strcmp(c, "private") == 0);
      snprintf(l->opponent, sizeof(l->opponent), "%s",
               strcmp(d, "-") == 0 ? "" : d);
      l->spectators = v1;
    }
    if (g_ui.in_lobby && strcmp(a, g_ui.lobby_leader) == 0) {
      g_ui.is_private = (strcmp(c, "private") == 0);
      snprintf(g_ui.lobby_opponent, sizeof(g_ui.lobby_opponent), "%s",
               strcmp(d, "-") == 0 ? "" : d);
    }
    return;
  }

  if (sscanf(line, "FRIEND %16s %15s", a, b) == 2) {
    if (strcmp(b, "incoming") == 0) {
      if (g_ui.num_friend_reqs_in < MAX_USERS) {
        snprintf(g_ui.friend_reqs_in[g_ui.num_friend_reqs_in++], NAME_LEN + 1,
                 "%s", a);
      }
    } else if (strcmp(b, "outgoing") == 0) {
      if (g_ui.num_friend_reqs_out < MAX_USERS) {
        snprintf(g_ui.friend_reqs_out[g_ui.num_friend_reqs_out++], NAME_LEN + 1,
                 "%s", a);
      }
    } else if (g_ui.num_friends < MAX_USERS) {
      FriendEntry *f = &g_ui.friends[g_ui.num_friends++];
      snprintf(f->name, sizeof(f->name), "%s", a);
      f->online = (strcmp(b, "online") == 0);
    }
    return;
  }

  if (sscanf(line, "RANK %d %16s %d %d %d", &v1, a, &v2, &v3, &v4) == 5) {
    if (g_ui.num_rankings < MAX_USERS) {
      RankEntry *r = &g_ui.rankings[g_ui.num_rankings++];
      r->pos = v1;
      snprintf(r->name, sizeof(r->name), "%s", a);
      r->wins = v2;
      r->losses = v3;
      r->draws = v4;
      r->points = v2 * 3 + v4;
    }
    return;
  }

  if (sscanf(line, "GAME %d %16s %16s %16s %d %d %d", &v1, a, b, c, &v2, &v3,
             &v4) == 7) {
    if (g_ui.num_history < HISTORY_CACHE_MAX) {
      HistoryEntry *h = &g_ui.history[g_ui.num_history++];
      h->id = v1;
      snprintf(h->p0, sizeof(h->p0), "%s", a);
      snprintf(h->p1, sizeof(h->p1), "%s", b);
      snprintf(h->winner, sizeof(h->winner), "%s", c);
      h->cap0 = v2;
      h->cap1 = v3;
      h->moves = v4;
    }
    return;
  }

  if (starts_with(line, "BIO ")) {
    if (bio_is_self && g_ui.bio_count < BIO_LINES) {
      snprintf(g_ui.bio_lines[g_ui.bio_count++], BIO_LINE_LEN + 1, "%s",
               line + 4);
    }
    if (bio_chat_lines_left > 0) {
      ui_state_add_chat(CHAT_SYSTEM, "Bio", line + 4);
      bio_chat_lines_left--;
    }
    return;
  }

  /* OK replies */
  if (starts_with(line, "OK help")) {
    const char *cmds = (strlen(line) > 8) ? (line + 8) : "";
    ui_state_add_chat(CHAT_SYSTEM, "Help", cmds);
    return;
  }
  if (sscanf(line, "OK login %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "Logged in as %s", a);
    ui_state_set_status(msg, 0);
    send_line(g_ui.sock, "/rank");
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "OK list %d", &v1) == 1) {
    g_ui.num_online = 0;
    if (g_ui.in_lobby)
      g_ui.num_spectators = 0;
    return;
  }

  if (sscanf(line, "OK lobbies %d", &v1) == 1) {
    g_ui.num_lobbies = 0;
    return;
  }

  if (sscanf(line, "OK friends %d", &v1) == 1) {
    g_ui.num_friends = 0;
    g_ui.num_friend_reqs_in = 0;
    g_ui.num_friend_reqs_out = 0;
    return;
  }

  if (sscanf(line, "OK rank %d", &v1) == 1) {
    g_ui.num_rankings = 0;
    return;
  }

  if (sscanf(line, "OK history %d", &v1) == 1) {
    g_ui.num_history = 0;
    return;
  }

  if (sscanf(line, "OK bio %16s %d", a, &v1) == 2) {
    bio_is_self = (strcmp(a, g_ui.me) == 0);
    if (bio_is_self) {
      snprintf(g_ui.bio_owner, sizeof(g_ui.bio_owner), "%s", a);
      g_ui.bio_count = 0;
    }
    if (bio_from_chat || !bio_is_self) {
      bio_from_chat = 0;
      bio_chat_lines_left = v1;
      if (v1 == 0) {
        snprintf(msg, sizeof(msg), "Bio of %s: (empty)", a);
        ui_state_add_chat(CHAT_SYSTEM, "Bio", msg);
      } else {
        snprintf(msg, sizeof(msg), "Bio of %s:", a);
        ui_state_add_chat(CHAT_SYSTEM, "Bio", msg);
      }
    }
    return;
  }

  if (strcmp(line, "OK bio") == 0) {
    ui_state_set_status("Bio updated!", 0);
    send_line(g_ui.sock, "/bio");
    return;
  }

  if (sscanf(line, "OK friend %16s", a) == 1) {
    if (ui_state_friend_status(a) == 2)
      snprintf(msg, sizeof(msg), "Accepted friend request from %s", a);
    else
      snprintf(msg, sizeof(msg), "Friend request sent to %s", a);
    ui_state_set_status(msg, 0);
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "OK unfriend %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "Removed %s from friends/requests", a);
    ui_state_set_status(msg, 0);
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "OK create %16s %15s", a, b) == 2) {
    g_ui.in_lobby = 1;
    g_ui.is_leader = 1;
    snprintf(g_ui.lobby_leader, sizeof(g_ui.lobby_leader), "%s", a);
    g_ui.is_private = (strcmp(b, "private") == 0);
    g_ui.lobby_opponent[0] = '\0';
    g_ui.num_spectators = 0;
    g_ui.has_invite = 0;
    if (g_ui.screen != SCREEN_LOBBY_LEADER)
      ui_state_set_screen(SCREEN_LOBBY_LEADER);
    return;
  }

  if (sscanf(line, "OK join %16s %15s", a, b) == 2) {
    g_ui.in_lobby = 1;
    g_ui.is_leader = 0;
    snprintf(g_ui.lobby_leader, sizeof(g_ui.lobby_leader), "%s", a);
    g_ui.has_invite = 0;
    g_ui.num_spectators = 0;
    if (strcmp(b, "player") == 0)
      snprintf(g_ui.lobby_opponent, sizeof(g_ui.lobby_opponent), "%s", g_ui.me);
    else
      g_ui.lobby_opponent[0] = '\0';
    ui_state_set_screen(SCREEN_LOBBY_MEMBER);
    return;
  }

  if (strcmp(line, "OK leave") == 0) {
    g_ui.in_lobby = 0;
    g_ui.playing = 0;
    if (pending_recreate != 0) {
      int priv = (pending_recreate == 2);
      pending_recreate = 0;
      send_line(g_ui.sock, priv ? "/create private" : "/create");
      return;
    }
    return;
  }

  if (sscanf(line, "OK invite %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "Invited %s to lobby", a);
    ui_state_set_status(msg, 0);
    return;
  }

  if (sscanf(line, "OK deny %16s", a) == 1) {
    g_ui.has_invite = 0;
    ui_state_set_status("Invitation declined", 0);
    return;
  }

  if (sscanf(line, "OK kick %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "Kicked %s", a);
    ui_state_set_status(msg, 0);
    if (strcmp(g_ui.lobby_opponent, a) == 0)
      g_ui.lobby_opponent[0] = '\0';
    remove_spectator(a);
    send_line(g_ui.sock, "/lobbies");
    send_line(g_ui.sock, "/list");
    return;
  }

  if (sscanf(line, "OK replay %d %d %d %16s %16s", &v1, &v2, &v3, a, b) == 5) {
    g_ui.replay_id = v1;
    g_ui.replay_step = v2;
    g_ui.replay_total = v3;
    snprintf(g_ui.replay_players[0], NAME_LEN + 1, "%s", a);
    snprintf(g_ui.replay_players[1], NAME_LEN + 1, "%s", b);
    return;
  }

  if (starts_with(line, "OK say") || starts_with(line, "OK msg") ||
      starts_with(line, "OK move") || starts_with(line, "OK start") ||
      starts_with(line, "OK forfeit")) {
    return;
  }

  /* Server Events (EVT) */
  if (sscanf(line, "EVT CHAT %16s %n", a, &n) == 1) {
    ui_state_add_chat(CHAT_PUBLIC, a, line + n);
    return;
  }

  if (sscanf(line, "EVT PM %16s %n", a, &n) == 1) {
    ui_state_add_chat(CHAT_PM, a, line + n);
    return;
  }

  if (sscanf(line, "EVT USER_JOIN %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "%s connected", a);
    ui_state_add_chat(CHAT_SYSTEM, "Server", msg);
    if (g_ui.screen == SCREEN_ONLINE_PLAYERS)
      send_line(g_ui.sock, "/list");
    else if (g_ui.screen == SCREEN_FRIENDS)
      send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "EVT USER_LEFT %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "%s disconnected", a);
    ui_state_add_chat(CHAT_SYSTEM, "Server", msg);
    if (g_ui.screen == SCREEN_ONLINE_PLAYERS)
      send_line(g_ui.sock, "/list");
    else if (g_ui.screen == SCREEN_FRIENDS)
      send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "EVT LOBBY_OPEN %16s %15s", a, b) == 2) {
    if (g_ui.screen == SCREEN_JOIN_LOBBY)
      send_line(g_ui.sock, "/lobbies");
    return;
  }

  if (sscanf(line, "EVT LOBBY_CLOSED %16s %16s", a, b) == 2) {
    if (g_ui.in_lobby && strcmp(g_ui.lobby_leader, a) == 0) {
      g_ui.in_lobby = 0;
      g_ui.playing = 0;
      ui_state_set_status("Lobby was closed by leader", 1);
      ui_state_set_screen(SCREEN_PLAY_MENU);
    } else if (g_ui.screen == SCREEN_JOIN_LOBBY) {
      send_line(g_ui.sock, "/lobbies");
    }
    return;
  }

  if (sscanf(line, "EVT JOINED %16s %16s %15s", a, b, c) == 3) {
    if (g_ui.in_lobby && strcmp(g_ui.lobby_leader, a) == 0) {
      if (strcmp(c, "player") == 0)
        snprintf(g_ui.lobby_opponent, sizeof(g_ui.lobby_opponent), "%s", b);
      else
        add_spectator(b);
      snprintf(msg, sizeof(msg), "%s joined lobby as %s", b, c);
      ui_state_add_chat(CHAT_SYSTEM, "Lobby", msg);
    }
    return;
  }

  if (sscanf(line, "EVT LEFT %16s %16s %16s", a, b, c) == 3) {
    if (g_ui.in_lobby && strcmp(g_ui.lobby_leader, a) == 0) {
      if (strcmp(g_ui.lobby_opponent, b) == 0)
        g_ui.lobby_opponent[0] = '\0';
      remove_spectator(b);
      snprintf(msg, sizeof(msg), "%s left lobby (%s)", b, c);
      ui_state_add_chat(CHAT_SYSTEM, "Lobby", msg);
    }
    return;
  }

  if (sscanf(line, "EVT SEAT %16s %16s", a, b) == 2) {
    if (g_ui.in_lobby && strcmp(g_ui.lobby_leader, a) == 0) {
      remove_spectator(b);
      snprintf(g_ui.lobby_opponent, sizeof(g_ui.lobby_opponent), "%s", b);
      snprintf(msg, sizeof(msg), "%s moved to player seat", b);
      ui_state_add_chat(CHAT_SYSTEM, "Lobby", msg);
    }
    return;
  }

  if (sscanf(line, "EVT KICKED %16s", a) == 1) {
    g_ui.in_lobby = 0;
    g_ui.playing = 0;
    ui_state_set_status("You were kicked from the lobby", 1);
    ui_state_set_screen(SCREEN_PLAY_MENU);
    return;
  }

  if (sscanf(line, "EVT INVITE_%15s %16s %16s", a, b, c) == 3) {
    if (strcmp(c, g_ui.me) == 0) {
      g_ui.has_invite = 0;
      snprintf(msg, sizeof(msg), "Invite from %s %s", b,
               strcmp(a, "EXPIRED") == 0 ? "expired" : "cancelled");
      ui_state_set_status(msg, 0);
    } else {
      snprintf(msg, sizeof(msg), "Invite to %s %s", c,
               strcmp(a, "DENIED") == 0    ? "was declined"
               : strcmp(a, "EXPIRED") == 0 ? "expired"
                                           : "was cancelled");
      ui_state_set_status(msg, 0);
      ui_state_add_chat(CHAT_SYSTEM, "Invite", msg);
    }
    return;
  }

  if (sscanf(line, "EVT INVITE %16s %d", a, &v1) == 2) {
    g_ui.has_invite = 1;
    snprintf(g_ui.invite_from, sizeof(g_ui.invite_from), "%s", a);
    g_ui.invite_duration = v1;
    g_ui.invite_received_at = time(NULL);
    snprintf(msg, sizeof(msg), "%s invited you to their lobby!", a);
    ui_state_set_status(msg, 0);
    ui_state_add_chat(CHAT_SYSTEM, "Invite", msg);
    return;
  }

  if (sscanf(line, "EVT FRIEND_REQ %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "%s sent you a friend request!", a);
    ui_state_set_status(msg, 0);
    ui_state_add_chat(CHAT_SYSTEM, "Friends", msg);
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "EVT FRIEND_ACCEPT %16s", a) == 1) {
    snprintf(msg, sizeof(msg), "%s accepted your friend request!", a);
    ui_state_set_status(msg, 0);
    ui_state_add_chat(CHAT_SYSTEM, "Friends", msg);
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "EVT UNFRIEND %16s", a) == 1) {
    send_line(g_ui.sock, "/friend");
    return;
  }

  if (sscanf(line, "EVT START %16s %16s %16s %15s %16s", a, b, c, d, e) == 5) {
    g_ui.playing = 1;
    snprintf(g_ui.game_players[0], NAME_LEN + 1, "%s", b);
    snprintf(g_ui.game_players[1], NAME_LEN + 1, "%s", c);
    g_ui.my_side = (strcmp(d, "spectator") == 0) ? -1 : (d[0] - '0');
    g_ui.selected_pit = 0;
    g_ui.game_end_info[0] = '\0';
    snprintf(g_ui.last_move_info, sizeof(g_ui.last_move_info),
             "Match started! %s begins.", e);
    ui_state_set_screen(SCREEN_ONLINE_GAME);
    return;
  }

  if (sscanf(line, "EVT MOVE %16s %16s %d", a, b, &v1) == 3) {
    snprintf(g_ui.last_move_info, sizeof(g_ui.last_move_info),
             "%s played pit #%d", b, v1);
    return;
  }

  if (sscanf(line, "EVT END %16s %16s %d %d %16s %d", a, b, &v1, &v2, c, &v3) ==
      6) {
    g_ui.playing = 0;
    if (strcmp(b, "draw") == 0) {
      snprintf(g_ui.game_end_info, sizeof(g_ui.game_end_info),
               "Game Over: Draw (%d - %d) [%s]", v1, v2, c);
    } else {
      snprintf(g_ui.game_end_info, sizeof(g_ui.game_end_info),
               "Game Over: %s wins (%d - %d) [%s]", b, v1, v2, c);
    }
    ui_state_add_chat(CHAT_SYSTEM, "Match", g_ui.game_end_info);
    return;
  }
}
