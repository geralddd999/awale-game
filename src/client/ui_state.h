#ifndef UI_STATE_H
#define UI_STATE_H

#include "game.h"
#include "net.h"
#include "protocol.h"
#include <time.h>

#define CHAT_MAX_LINES 128
#define CHAT_LINE_LEN 256
#define HISTORY_CACHE_MAX 32

typedef enum {
  SCREEN_MAIN_MENU = 0,
  SCREEN_PROFILE,
  SCREEN_FRIENDS,
  SCREEN_ONLINE_PLAYERS,
  SCREEN_RANKING,
  SCREEN_REPLAYS,
  SCREEN_REPLAY_VIEW,
  SCREEN_PLAY_MENU,
  SCREEN_LOCAL_GAME,
  SCREEN_JOIN_LOBBY,
  SCREEN_LOBBY_LEADER,
  SCREEN_LOBBY_MEMBER,
  SCREEN_ONLINE_GAME
} ScreenId;

typedef enum {
  FOCUS_MAIN = 0,
  FOCUS_SIDE,
  FOCUS_CHAT,
  FOCUS_INVITES,
  FOCUS_MODAL
} FocusArea;

typedef enum {
  MODAL_NONE = 0,
  MODAL_EDIT_BIO,
  MODAL_ADD_FRIEND,
  MODAL_INVITE_USER,
  MODAL_PENDING_INVITES
} ModalType;

typedef enum { CHAT_PUBLIC = 0, CHAT_PM, CHAT_SYSTEM, CHAT_ERROR } ChatKind;

typedef struct {
  ChatKind kind;
  char sender[NAME_LEN + 1];
  char text[CHAT_LINE_LEN];
} ChatEntry;

typedef struct {
  char name[NAME_LEN + 1];
  int online;
} FriendEntry;

typedef struct {
  char name[NAME_LEN + 1];
  int in_lobby;
  char lobby[NAME_LEN + 1];
} OnlineUser;

typedef struct {
  int pos;
  char name[NAME_LEN + 1];
  int wins;
  int losses;
  int draws;
  int points;
} RankEntry;

typedef struct {
  char leader[NAME_LEN + 1];
  int playing;
  int is_private;
  char opponent[NAME_LEN + 1];
  int spectators;
} LobbyInfo;

typedef struct {
  int id;
  char p0[NAME_LEN + 1];
  char p1[NAME_LEN + 1];
  char winner[NAME_LEN + 1];
  int cap0;
  int cap1;
  int moves;
} HistoryEntry;

typedef struct {
  SOCKET sock;
  int running;
  int needs_redraw;
  char me[NAME_LEN + 1];

  ScreenId screen;
  FocusArea focus;
  ModalType modal;

  /* Selection indices */
  int main_idx;
  int side_idx;
  int list_scroll;
  int sub_action_idx;
  int modal_idx;

  /* Status notification bar */
  char status_msg[128];
  int status_is_err;
  time_t status_time;

  /* Chat panel */
  ChatEntry chat[CHAT_MAX_LINES];
  int chat_count;
  int chat_scroll;
  char chat_input[CHAT_LINE_LEN];
  int chat_cursor;

  /* Pending lobby invitation */
  int has_invite;
  char invite_from[NAME_LEN + 1];
  int invite_duration;
  time_t invite_received_at;

  /* Profile & Bio */
  char bio_owner[NAME_LEN + 1];
  char bio_lines[BIO_LINES][BIO_LINE_LEN + 1];
  int bio_count;
  char bio_edit[BIO_LINES][BIO_LINE_LEN + 1];
  int bio_edit_line;
  int bio_edit_col;
  int bio_edit_num_lines;

  /* Modal text input */
  char modal_input[NAME_LEN + 1];
  int modal_cursor;

  /* Cached server lists */
  FriendEntry friends[MAX_USERS];
  int num_friends;
  char friend_reqs_in[MAX_USERS][NAME_LEN + 1];
  int num_friend_reqs_in;
  char friend_reqs_out[MAX_USERS][NAME_LEN + 1];
  int num_friend_reqs_out;

  OnlineUser online_users[MAX_CLIENTS];
  int num_online;

  RankEntry rankings[MAX_USERS];
  int num_rankings;

  LobbyInfo lobbies[MAX_LOBBIES];
  int num_lobbies;

  HistoryEntry history[HISTORY_CACHE_MAX];
  int num_history;

  /* Current Lobby state */
  int in_lobby;
  char lobby_leader[NAME_LEN + 1];
  int is_leader;
  int is_private;
  char lobby_opponent[NAME_LEN + 1];
  char lobby_spectators[MAX_LOBBY_MEMBERS][NAME_LEN + 1];
  int num_spectators;

  /* Online Game state */
  int playing;
  Board online_board;
  char game_players[2][NAME_LEN + 1];
  int my_side; /* 0, 1, or -1 for spectator */
  int selected_pit;
  char last_move_info[128];
  char game_end_info[128];

  /* Local Game state */
  Board local_board;
  int local_selected_pit;
  char local_info[128];

  /* Replay state */
  int replay_id;
  int replay_step;
  int replay_total;
  int replay_autoplay;
  long long replay_last_step_ms;
  char replay_players[2][NAME_LEN + 1];
  Board replay_board;
} UIState;

extern UIState g_ui;

void ui_state_init(const char *username, SOCKET sock);
void ui_state_set_screen(ScreenId screen);
void ui_state_set_status(const char *msg, int is_err);
void ui_state_add_chat(ChatKind kind, const char *sender, const char *text);
int ui_state_invite_remaining(void);
int ui_state_user_points(const char *name);
int ui_state_is_friend(const char *name);
/* 0 = none, 1 = mutual friend, 2 = incoming request, 3 = outgoing request */
int ui_state_friend_status(const char *name);
long long ui_now_ms(void);

#endif
