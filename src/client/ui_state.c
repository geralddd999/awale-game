#include "ui_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

UIState g_ui;

long long ui_now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000LL + (tv.tv_usec / 1000);
}

void ui_state_init(const char *username, SOCKET sock) {
  memset(&g_ui, 0, sizeof(g_ui));
  g_ui.sock = sock;
  g_ui.running = 1;
  g_ui.needs_redraw = 1;
  snprintf(g_ui.me, sizeof(g_ui.me), "%s", username);

  g_ui.screen = SCREEN_MAIN_MENU;
  g_ui.focus = FOCUS_MAIN;
  g_ui.modal = MODAL_NONE;
  g_ui.my_side = -1;

  init_board(&g_ui.local_board, PLAYER_0);
  init_board(&g_ui.online_board, PLAYER_0);
  init_board(&g_ui.replay_board, PLAYER_0);

  ui_state_add_chat(CHAT_SYSTEM, "System",
                    "Welcome to Awale! Press Tab to chat.");
}

void ui_state_set_screen(ScreenId screen) {
  g_ui.screen = screen;
  g_ui.main_idx = 0;
  g_ui.side_idx = 0;
  g_ui.list_scroll = 0;
  g_ui.sub_action_idx = 0;
  g_ui.modal = MODAL_NONE;
  if (g_ui.focus != FOCUS_CHAT)
    g_ui.focus = FOCUS_MAIN;
  g_ui.needs_redraw = 1;

  if (g_ui.sock == INVALID_SOCKET)
    return;

  switch (screen) {
  case SCREEN_MAIN_MENU:
    break;
  case SCREEN_PROFILE:
    send_line(g_ui.sock, "/bio");
    send_line(g_ui.sock, "/rank");
    break;
  case SCREEN_FRIENDS:
    send_line(g_ui.sock, "/friend");
    break;
  case SCREEN_ONLINE_PLAYERS:
    send_line(g_ui.sock, "/list");
    send_line(g_ui.sock, "/rank");
    send_line(g_ui.sock, "/friend");
    break;
  case SCREEN_RANKING:
    send_line(g_ui.sock, "/rank");
    send_line(g_ui.sock, "/friend");
    break;
  case SCREEN_REPLAYS:
    send_line(g_ui.sock, "/history");
    break;
  case SCREEN_JOIN_LOBBY:
    send_line(g_ui.sock, "/lobbies");
    break;
  case SCREEN_LOBBY_LEADER:
  case SCREEN_LOBBY_MEMBER:
    send_line(g_ui.sock, "/lobbies");
    send_line(g_ui.sock, "/list");
    break;
  case SCREEN_LOCAL_GAME:
    init_board(&g_ui.local_board, (Side)(rand() % 2));
    g_ui.local_selected_pit = 0;
    snprintf(g_ui.local_info, sizeof(g_ui.local_info),
             "Local hotseat match started! Player %d begins.",
             g_ui.local_board.turn + 1);
    break;
  default:
    break;
  }
}

void ui_state_set_status(const char *msg, int is_err) {
  snprintf(g_ui.status_msg, sizeof(g_ui.status_msg), "%s", msg);
  g_ui.status_is_err = is_err;
  g_ui.status_time = time(NULL);
  g_ui.needs_redraw = 1;
}

void ui_state_add_chat(ChatKind kind, const char *sender, const char *text) {
  if (g_ui.chat_count == CHAT_MAX_LINES) {
    memmove(&g_ui.chat[0], &g_ui.chat[1],
            sizeof(ChatEntry) * (CHAT_MAX_LINES - 1));
    g_ui.chat_count--;
  }
  ChatEntry *e = &g_ui.chat[g_ui.chat_count++];
  e->kind = kind;
  snprintf(e->sender, sizeof(e->sender), "%s", sender ? sender : "");
  snprintf(e->text, sizeof(e->text), "%s", text ? text : "");
  g_ui.chat_scroll = 0;
  g_ui.needs_redraw = 1;
}

int ui_state_invite_remaining(void) {
  if (!g_ui.has_invite)
    return 0;
  int elapsed = (int)(time(NULL) - g_ui.invite_received_at);
  int rem = g_ui.invite_duration - elapsed;
  if (rem <= 0) {
    g_ui.has_invite = 0;
    return 0;
  }
  return rem;
}

int ui_state_user_points(const char *name) {
  for (int i = 0; i < g_ui.num_rankings; i++) {
    if (strcmp(g_ui.rankings[i].name, name) == 0)
      return g_ui.rankings[i].points;
  }
  return 0;
}

int ui_state_is_friend(const char *name) {
  for (int i = 0; i < g_ui.num_friends; i++) {
    if (strcmp(g_ui.friends[i].name, name) == 0)
      return 1;
  }
  return 0;
}
