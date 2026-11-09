#include "ui.h"
#include "net_client.h"
#include "ui_board.h"
#include "ui_state.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void draw_box_style(WINDOW *w, int y, int x, int h, int width,
                           int dashed, int active) {
  int pair = active ? CP_BORDER_ACTIVE : CP_BORDER;
  wattron(w, COLOR_PAIR(pair) | (active ? A_BOLD : 0));
  if (dashed) {
    for (int col = x + 1; col < x + width - 1; col++) {
      mvwaddch(w, y, col, (col % 2 == 0) ? '-' : ' ');
      mvwaddch(w, y + h - 1, col, (col % 2 == 0) ? '-' : ' ');
    }
    for (int row = y + 1; row < y + h - 1; row++) {
      mvwaddch(w, row, x, (row % 2 == 0) ? ':' : ' ');
      mvwaddch(w, row, x + width - 1, (row % 2 == 0) ? ':' : ' ');
    }
    mvwaddch(w, y, x, '+');
    mvwaddch(w, y, x + width - 1, '+');
    mvwaddch(w, y + h - 1, x, '+');
    mvwaddch(w, y + h - 1, x + width - 1, '+');
  } else {
    mvwhline(w, y, x + 1, ACS_HLINE, width - 2);
    mvwhline(w, y + h - 1, x + 1, ACS_HLINE, width - 2);
    mvwvline(w, y + 1, x, ACS_VLINE, h - 2);
    mvwvline(w, y + 1, x + width - 1, ACS_VLINE, h - 2);
    mvwaddch(w, y, x, ACS_ULCORNER);
    mvwaddch(w, y, x + width - 1, ACS_URCORNER);
    mvwaddch(w, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(w, y + h - 1, x + width - 1, ACS_LRCORNER);
  }
  wattroff(w, COLOR_PAIR(pair) | (active ? A_BOLD : 0));
}

static void print_button(WINDOW *w, int y, int x, const char *label,
                         int selected) {
  if (selected) {
    wattron(w, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    mvwprintw(w, y, x, "> %s <", label);
    wattroff(w, COLOR_PAIR(CP_SELECTED) | A_BOLD);
  } else {
    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, y, x, "  %s  ", label);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));
  }
}

void ui_init(void) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  set_escdelay(25);
  curs_set(0);
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

  struct termios tio;
  if (tcgetattr(STDIN_FILENO, &tio) == 0) {
    tio.c_iflag &= ~(IXON | IXOFF);
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
  }

  define_key("\033[A", KEY_UP);
  define_key("\033[B", KEY_DOWN);
  define_key("\033[C", KEY_RIGHT);
  define_key("\033[D", KEY_LEFT);
  define_key("\033OA", KEY_UP);
  define_key("\033OB", KEY_DOWN);
  define_key("\033OC", KEY_RIGHT);
  define_key("\033OD", KEY_LEFT);

  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT, -1, -1);
    init_pair(CP_TITLE, COLOR_CYAN, -1);
    init_pair(CP_SELECTED, COLOR_BLACK, COLOR_CYAN);
    init_pair(CP_BORDER, COLOR_WHITE, -1);
    init_pair(CP_BORDER_ACTIVE, COLOR_YELLOW, -1);
    init_pair(CP_CHAT_NAME, COLOR_GREEN, -1);
    init_pair(CP_CHAT_PM, COLOR_MAGENTA, -1);
    init_pair(CP_CHAT_SYS, COLOR_YELLOW, -1);
    init_pair(CP_ERROR, COLOR_RED, -1);
    init_pair(CP_SUCCESS, COLOR_GREEN, -1);
    init_pair(CP_PIT, COLOR_WHITE, -1);
    init_pair(CP_PIT_ACTIVE, COLOR_CYAN, -1);
    init_pair(CP_STORE, COLOR_YELLOW, -1);
  }
}

void ui_shutdown(void) { endwin(); }

void ui_tick(void) {
  static time_t last_sec = 0;
  time_t now = time(NULL);
  if (now != last_sec) {
    last_sec = now;
    if (g_ui.has_invite) {
      ui_state_invite_remaining();
      g_ui.needs_redraw = 1;
    }
    if (g_ui.status_msg[0] != '\0' && (now - g_ui.status_time) >= 5) {
      g_ui.status_msg[0] = '\0';
      g_ui.needs_redraw = 1;
    }
  }

  /* Autoplay logic for Replay screen */
  if (g_ui.screen == SCREEN_REPLAY_VIEW && g_ui.replay_autoplay) {
    long long ms = ui_now_ms();
    if (ms - g_ui.replay_last_step_ms >= 1100) {
      g_ui.replay_last_step_ms = ms;
      if (g_ui.replay_step < g_ui.replay_total) {
        send_line(g_ui.sock, "/replay %d %d", g_ui.replay_id,
                  g_ui.replay_step + 1);
      } else {
        g_ui.replay_autoplay = 0;
        g_ui.needs_redraw = 1;
      }
    }
  }
}

typedef struct {
  int color;
  char text[128];
} WrappedChatLine;

/* Draw persistent dashed Chat box with full line wrapping */
static void render_chat_box(int y, int x, int h, int w) {
  int active = (g_ui.focus == FOCUS_CHAT);
  draw_box_style(stdscr, y, x, h, w, 1, active);

  wattron(stdscr, COLOR_PAIR(active ? CP_BORDER_ACTIVE : CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y, x + 2, " Chat %s", active ? "[Typing]" : "[Tab]");
  wattroff(stdscr, COLOR_PAIR(active ? CP_BORDER_ACTIVE : CP_TITLE) | A_BOLD);

  int inner_w = w - 2;
  if (inner_w < 4)
    return;

  /* Calculate how many rows the input box needs (1 or 2 rows wrapped) */
  int input_len = (int)strlen(g_ui.chat_input);
  int first_cap = inner_w - 1; /* after '>' */
  int input_rows = (active && input_len > first_cap) ? 2 : 1;

  int msg_rows = h - 3 - input_rows;
  if (msg_rows < 1)
    return;

  /* Wrap recent chat messages into visual lines of width `inner_w` */
  static WrappedChatLine vlines[CHAT_MAX_LINES * 8];
  int nvlines = 0;
  int first_entry = g_ui.chat_count - msg_rows;
  if (first_entry < 0)
    first_entry = 0;

  for (int i = first_entry; i < g_ui.chat_count; i++) {
    ChatEntry *e = &g_ui.chat[i];
    char line_buf[CHAT_LINE_LEN + 32];
    if (e->kind == CHAT_PM)
      snprintf(line_buf, sizeof(line_buf), "(PM)%s: %s", e->sender, e->text);
    else if (e->kind == CHAT_PUBLIC)
      snprintf(line_buf, sizeof(line_buf), "%s: %s", e->sender, e->text);
    else
      snprintf(line_buf, sizeof(line_buf), "* %s", e->text);

    int color = CP_DEFAULT;
    if (e->kind == CHAT_PUBLIC)
      color = CP_CHAT_NAME;
    else if (e->kind == CHAT_PM)
      color = CP_CHAT_PM;
    else if (e->kind == CHAT_SYSTEM)
      color = CP_CHAT_SYS;
    else if (e->kind == CHAT_ERROR)
      color = CP_ERROR;

    const char *p = line_buf;
    int is_continuation = 0;
    while (*p != '\0' && nvlines < (int)(sizeof(vlines) / sizeof(vlines[0]))) {
      int avail = is_continuation ? (inner_w - 2) : inner_w;
      if (avail < 1)
        avail = 1;
      int rem_len = (int)strlen(p);
      int take = rem_len;

      if (take > avail) {
        take = avail;
        /* Prefer breaking at a space if one exists in the second half of the
         * line */
        for (int k = avail; k > avail / 3; k--) {
          if (p[k] == ' ') {
            take = k;
            break;
          }
        }
      }

      WrappedChatLine *vl = &vlines[nvlines++];
      vl->color = color;
      if (is_continuation) {
        snprintf(vl->text, sizeof(vl->text), "  %.*s", take, p);
      } else {
        snprintf(vl->text, sizeof(vl->text), "%.*s", take, p);
      }

      p += take;
      while (*p == ' ')
        p++;
      is_continuation = 1;
    }
  }

  int start_v = nvlines - msg_rows;
  if (start_v < 0)
    start_v = 0;

  for (int i = 0; i < msg_rows && (start_v + i) < nvlines; i++) {
    WrappedChatLine *vl = &vlines[start_v + i];
    wattron(stdscr, COLOR_PAIR(vl->color));
    mvwprintw(stdscr, y + 1 + i, x + 1, "%-*.*s", inner_w, inner_w, vl->text);
    wattroff(stdscr, COLOR_PAIR(vl->color));
  }

  int sep_row = y + h - 2 - input_rows;
  mvwhline(stdscr, sep_row, x + 1, ACS_HLINE, inner_w);

  int input_row = sep_row + 1;
  if (active) {
    wattron(stdscr, COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
    if (input_rows == 1) {
      mvwprintw(stdscr, input_row, x + 1, ">%-*.*s", first_cap, first_cap,
                g_ui.chat_input);
    } else {
      /* Wrap long chat input across 2 lines */
      int total_cap = first_cap + inner_w;
      const char *src = g_ui.chat_input;
      if (input_len > total_cap)
        src += (input_len - total_cap);
      mvwprintw(stdscr, input_row, x + 1, ">%-*.*s", first_cap, first_cap, src);
      mvwprintw(stdscr, input_row + 1, x + 1, "%-*.*s", inner_w, inner_w,
                src + first_cap);
    }
    wattroff(stdscr, COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
  } else {
    mvwprintw(stdscr, input_row, x + 1, ">%-*.*s", first_cap, first_cap,
              input_len > 0 ? g_ui.chat_input : "Press Tab to chat");
  }
}

/* Draw persistent dashed Invites box on the bottom-right */
static void render_invites_box(int y, int x, int h, int w) {
  int rem = ui_state_invite_remaining();
  int has_freq = (g_ui.num_friend_reqs_in > 0);
  int active = (g_ui.focus == FOCUS_INVITES) || (rem > 0) || has_freq;
  draw_box_style(stdscr, y, x, h, w, 1, active);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y, x + 2, " Invites ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int inner_w = w - 2;
  if (rem > 0) {
    wattron(stdscr, COLOR_PAIR(CP_CHAT_SYS) | A_BOLD);
    mvwprintw(stdscr, y + 2, x + 2, "Lobby: %-*.*s", inner_w - 8, inner_w - 8,
              g_ui.invite_from);
    mvwprintw(stdscr, y + 3, x + 2, "Expires: %ds", rem);
    wattroff(stdscr, COLOR_PAIR(CP_CHAT_SYS) | A_BOLD);

    wattron(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
    mvwprintw(stdscr, y + 5, x + 2, "[Ctrl-A] Accept");
    wattroff(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);

    wattron(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
    mvwprintw(stdscr, y + 6, x + 2, "[Ctrl-D] Deny");
    wattroff(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
  } else if (has_freq) {
    wattron(stdscr, COLOR_PAIR(CP_CHAT_SYS) | A_BOLD);
    mvwprintw(stdscr, y + 2, x + 2,
              "Friend Req (%d):", g_ui.num_friend_reqs_in);
    mvwprintw(stdscr, y + 3, x + 2, "From: %-*.*s", inner_w - 7, inner_w - 7,
              g_ui.friend_reqs_in[0]);
    wattroff(stdscr, COLOR_PAIR(CP_CHAT_SYS) | A_BOLD);

    wattron(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
    mvwprintw(stdscr, y + 5, x + 2, "[Ctrl-A] Accept");
    wattroff(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);

    wattron(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
    mvwprintw(stdscr, y + 6, x + 2, "[Ctrl-D] Reject");
    wattroff(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
  } else {
    mvwprintw(stdscr, y + h / 2, x + 2, "%-*.*s", inner_w - 1, inner_w - 1,
              "No pending invites");
  }
}

/* Modal rendering for Bio Edit, Add Friend, Invite Player, Pending Invites */
static void render_modal(int rows, int cols) {
  if (g_ui.modal == MODAL_NONE)
    return;

  if (g_ui.modal == MODAL_EDIT_BIO) {
    int mh = 16, mw = 68;
    int my = (rows - mh) / 2;
    int mx = (cols - mw) / 2;
    for (int r = my; r < my + mh; r++)
      mvwhline(stdscr, r, mx, ' ', mw);
    draw_box_style(stdscr, my, mx, mh, mw, 0, 1);

    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(stdscr, my, mx + 2,
              " Edit Bio (Up/Down/Enter, Ctrl-S to Save, Esc to Cancel) ");
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    for (int i = 0; i < BIO_LINES; i++) {
      int is_cur = (i == g_ui.bio_edit_line);
      if (is_cur)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED));
      mvwprintw(stdscr, my + 2 + i, mx + 2, "%2d: %-60.60s", i + 1,
                g_ui.bio_edit[i]);
      if (is_cur)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED));
    }
    mvwprintw(stdscr, my + mh - 2, mx + 2,
              "[Ctrl-S / F2] Save Bio    [Esc] Cancel");
    return;
  }

  if (g_ui.modal == MODAL_ADD_FRIEND || g_ui.modal == MODAL_INVITE_USER) {
    int mh = 7, mw = 46;
    int my = (rows - mh) / 2;
    int mx = (cols - mw) / 2;
    for (int r = my; r < my + mh; r++)
      mvwhline(stdscr, r, mx, ' ', mw);
    draw_box_style(stdscr, my, mx, mh, mw, 0, 1);

    const char *title = (g_ui.modal == MODAL_ADD_FRIEND)
                            ? " Add Friend (Insert player name) "
                            : " Invite Player to Lobby ";
    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(stdscr, my, mx + 2, "%s", title);
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    mvwprintw(stdscr, my + 2, mx + 3, "Username: [ %-20s ]", g_ui.modal_input);
    mvwprintw(stdscr, my + 4, mx + 3, "[Enter] Confirm    [Esc] Cancel");
    return;
  }

  if (g_ui.modal == MODAL_PENDING_INVITES) {
    int mh = 14, mw = 56;
    int my = (rows - mh) / 2;
    int mx = (cols - mw) / 2;
    for (int r = my; r < my + mh; r++)
      mvwhline(stdscr, r, mx, ' ', mw);
    draw_box_style(stdscr, my, mx, mh, mw, 0, 1);

    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(stdscr, my, mx + 2, " Pending Friend & Lobby Invites ");
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    int rem = ui_state_invite_remaining();
    int total = g_ui.num_friend_reqs_in + (rem > 0 ? 1 : 0);
    if (g_ui.modal_idx >= total)
      g_ui.modal_idx = total > 0 ? total - 1 : 0;

    if (total == 0) {
      mvwprintw(stdscr, my + 4, mx + 3,
                "No pending friend or lobby invites at the moment.");
    } else {
      int row_offset = 0;
      for (int i = 0; i < g_ui.num_friend_reqs_in && row_offset < mh - 6; i++) {
        int sel = (g_ui.modal_idx == row_offset);
        if (sel)
          wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvwprintw(stdscr, my + 2 + row_offset, mx + 3,
                  "%d. Friend request from %-16s", row_offset + 1,
                  g_ui.friend_reqs_in[i]);
        if (sel)
          wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
        row_offset++;
      }
      if (rem > 0 && row_offset < mh - 6) {
        int sel = (g_ui.modal_idx == row_offset);
        if (sel)
          wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvwprintw(stdscr, my + 2 + row_offset, mx + 3,
                  "%d. Lobby invite from %-14s (%ds left)", row_offset + 1,
                  g_ui.invite_from, rem);
        if (sel)
          wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      }
      wattron(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
      mvwprintw(stdscr, my + mh - 4, mx + 3, "[A / Enter] Accept");
      wattroff(stdscr, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
      wattron(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
      mvwprintw(stdscr, my + mh - 4, mx + 26, "[D / X] Reject");
      wattroff(stdscr, COLOR_PAIR(CP_ERROR) | A_BOLD);
    }
    mvwprintw(stdscr, my + mh - 2, mx + 3, "[Up/Down] Select    [Esc] Close");
  }
}

/* Individual screen renderers */
static void render_screen_main_menu(int y, int x, int h, int w, int rx,
                                    int rw) {
  int cx = x + w / 2 - 8;
  int cy = y + 3;

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, cy, cx + 2, "=== AWALE ===");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  const char *main_items[4] = {"Play", "Ranking", "Replays", "Quit"};
  for (int i = 0; i < 4; i++) {
    int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);
    print_button(stdscr, cy + 3 + i * 2, cx + 1, main_items[i], sel);
  }

  /* Top-Right dashed box: Profile, Friends, Online players */
  int side_h = 9;
  draw_box_style(stdscr, y + 1, rx, side_h, rw, 1, g_ui.focus == FOCUS_SIDE);
  const char *side_items[3] = {"Profile", "Friends", "Online players"};
  for (int i = 0; i < 3; i++) {
    int sel = (g_ui.focus == FOCUS_SIDE && g_ui.side_idx == i);
    print_button(stdscr, y + 3 + i * 2, rx + 2, side_items[i], sel);
  }

  mvwprintw(stdscr, y + h - 2, x + 2,
            "Arrows: Navigate | Left/Right: Side Menu | Enter: Select");
}

static void render_screen_profile(int y, int x, int h, int w, int rx, int rw) {
  int card_w = w - 4;
  int card_h = h - 7;
  draw_box_style(stdscr, y + 1, x + 2, card_h, card_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  int pts = ui_state_user_points(g_ui.me);
  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 2, x + 4, "Profile Name: %s (%d pts)", g_ui.me, pts);
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  mvwhline(stdscr, y + 3, x + 3, ACS_HLINE, card_w - 2);
  mvwprintw(stdscr, y + 4, x + 4, "Bio:");
  if (g_ui.bio_count == 0) {
    mvwprintw(stdscr, y + 6, x + 6, "(Empty bio - select Edit Bio below)");
  } else {
    for (int i = 0; i < g_ui.bio_count && i < card_h - 6; i++) {
      mvwprintw(stdscr, y + 5 + i, x + 6, "%-.*s", card_w - 8,
                g_ui.bio_lines[i]);
    }
  }

  /* Edit Bio button below card */
  print_button(stdscr, y + card_h + 2, x + card_w / 2 - 6, "Edit Bio",
               g_ui.focus == FOCUS_MAIN && g_ui.main_idx == 0);

  /* Go back on right */
  print_button(stdscr, y + 5, rx + 2, "Go back",
               g_ui.focus == FOCUS_SIDE ||
                   (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == 1));
  (void)rw;
}

static void render_screen_friends(int y, int x, int h, int w, int rx, int rw) {
  int box_w = w - 4;
  int box_h = h - 3;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Friends List ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int max_rows = box_h - 5;
  int row_idx = 0;
  if (g_ui.num_friends == 0 && g_ui.num_friend_reqs_out == 0) {
    mvwprintw(stdscr, y + 3, x + 4, "No friends added yet.");
  } else {
    for (int i = 0; i < g_ui.num_friends && row_idx < max_rows; i++) {
      FriendEntry *f = &g_ui.friends[i];
      int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);
      if (sel)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(stdscr, y + 3 + row_idx, x + 4, "%d. %-16s (%s)  [X: remove?]",
                i + 1, f->name, f->online ? "online" : "offline");
      if (sel)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      row_idx++;
    }
    for (int i = 0; i < g_ui.num_friend_reqs_out && row_idx < max_rows; i++) {
      wattron(stdscr, COLOR_PAIR(CP_CHAT_SYS));
      mvwprintw(stdscr, y + 3 + row_idx, x + 4, "* %-16s (request sent...)",
                g_ui.friend_reqs_out[i]);
      wattroff(stdscr, COLOR_PAIR(CP_CHAT_SYS));
      row_idx++;
    }
  }

  /* Bottom of center box: Add friend? */
  int add_idx = g_ui.num_friends;
  print_button(stdscr, y + box_h - 1, x + 6, "Add friend? (A)",
               g_ui.focus == FOCUS_MAIN && g_ui.main_idx == add_idx);

  /* Right side: Pending invites & Go back */
  char pend_label[32];
  if (g_ui.num_friend_reqs_in > 0)
    snprintf(pend_label, sizeof(pend_label), "Pending (%d)",
             g_ui.num_friend_reqs_in);
  else
    snprintf(pend_label, sizeof(pend_label), "Pending invites");
  print_button(stdscr, y + 3, rx + 1, pend_label,
               g_ui.focus == FOCUS_SIDE && g_ui.side_idx == 0);
  print_button(stdscr, y + 6, rx + 1, "Go back",
               g_ui.focus == FOCUS_SIDE && g_ui.side_idx == 1);
  (void)rw;
}

static void render_screen_online_players(int y, int x, int h, int w, int rx,
                                         int rw) {
  int box_w = w - 4;
  int box_h = h - 3;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Online Players ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int max_rows = box_h - 4;
  if (g_ui.num_online == 0) {
    mvwprintw(stdscr, y + 3, x + 4, "Loading online players...");
  } else {
    for (int i = 0; i < g_ui.num_online && i < max_rows; i++) {
      OnlineUser *u = &g_ui.online_users[i];
      int pts = ui_state_user_points(u->name);
      int is_me = (strcmp(u->name, g_ui.me) == 0);
      int fst = ui_state_friend_status(u->name);
      int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);

      const char *tag = is_me      ? "(you)"
                        : fst == 1 ? "[friend]"
                        : fst == 2 ? "[accept req?]"
                        : fst == 3 ? "[req sent]"
                                   : "[Enter/A: add?]";

      if (sel)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(stdscr, y + 3 + i, x + 4, "%d. %-14s, %3d pts  %s", i + 1,
                u->name, pts, tag);
      if (sel)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    }
  }

  print_button(stdscr, y + 5, rx + 2, "Go back", g_ui.focus == FOCUS_SIDE);
  (void)rw;
}

static void render_screen_ranking(int y, int x, int h, int w, int rx, int rw) {
  int box_w = w - 4;
  int box_h = h - 3;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Ranking ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int max_rows = box_h - 4;
  if (g_ui.num_rankings == 0) {
    mvwprintw(stdscr, y + 3, x + 4, "No ranked players yet.");
  } else {
    for (int i = 0; i < g_ui.num_rankings && i < max_rows; i++) {
      RankEntry *r = &g_ui.rankings[i];
      int is_me = (strcmp(r->name, g_ui.me) == 0);
      int fst = ui_state_friend_status(r->name);
      int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);

      const char *tag = is_me      ? "(you)"
                        : fst == 1 ? "[friend]"
                        : fst == 2 ? "[accept?]"
                        : fst == 3 ? "[req sent]"
                                   : "[add?]";

      if (sel)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(stdscr, y + 3 + i, x + 4, "%d. %-13s %3d pts (%dW/%dL/%dD) %s",
                r->pos, r->name, r->points, r->wins, r->losses, r->draws, tag);
      if (sel)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    }
  }

  print_button(stdscr, y + 5, rx + 2, "Go back", g_ui.focus == FOCUS_SIDE);
  (void)rw;
}

static void render_screen_replays(int y, int x, int h, int w, int rx, int rw) {
  int box_w = w - 4;
  int box_h = h - 3;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Replays (Select game to autoplay) ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int max_rows = box_h - 4;
  if (g_ui.num_history == 0) {
    mvwprintw(stdscr, y + 3, x + 4, "No recorded games found.");
  } else {
    for (int i = 0; i < g_ui.num_history && i < max_rows; i++) {
      HistoryEntry *he = &g_ui.history[i];
      const char *against = (strcmp(he->p0, g_ui.me) == 0) ? he->p1 : he->p0;
      int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);

      if (sel)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(stdscr, y + 3 + i, x + 4,
                "%d. Game #%d, Against %-12s (%d-%d, %d moves)", i + 1, he->id,
                against, he->cap0, he->cap1, he->moves);
      if (sel)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    }
  }

  print_button(stdscr, y + 5, rx + 2, "Go back", g_ui.focus == FOCUS_SIDE);
  (void)rw;
}

static void render_screen_play_menu(int y, int x, int h, int w, int rx,
                                    int rw) {
  int cx = x + w / 2 - 8;
  int cy = y + 3;

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, cy, cx + 2, "=== AWALE ===");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  const char *items[4] = {"Play Local", "Create Lobby", "Join Lobby",
                          "Go back"};
  for (int i = 0; i < 4; i++) {
    int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);
    print_button(stdscr, cy + 3 + i * 2, cx, items[i], sel);
  }
  (void)h;
  (void)rx;
  (void)rw;
}

static void render_screen_join_lobby(int y, int x, int h, int w, int rx,
                                     int rw) {
  int box_w = w - 4;
  int box_h = h - 3;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Public Lobbies (Enter to Join) ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  int max_rows = box_h - 4;
  if (g_ui.num_lobbies == 0) {
    mvwprintw(stdscr, y + 3, x + 4, "No lobbies currently open.");
  } else {
    for (int i = 0; i < g_ui.num_lobbies && i < max_rows; i++) {
      LobbyInfo *l = &g_ui.lobbies[i];
      int as_spec = (l->opponent[0] != '\0' || l->playing);
      int sel = (g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);

      if (sel)
        wattron(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(stdscr, y + 3 + i, x + 4,
                "%d. %-14s [%s] %-7s (specs:%d) -> join as %s", i + 1,
                l->leader, l->is_private ? "priv" : "pub",
                l->playing ? "playing" : "waiting", l->spectators,
                as_spec ? "spec" : "player");
      if (sel)
        wattroff(stdscr, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    }
  }

  print_button(stdscr, y + 5, rx + 2, "Go back", g_ui.focus == FOCUS_SIDE);
  (void)rw;
}

static void render_screen_lobby_leader(int y, int x, int h, int w, int rx,
                                       int rw) {
  int box_w = w - 4;
  int box_h = h - 9;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Lobby Members ");
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  mvwprintw(stdscr, y + 3, x + 4, "1. %s (You - Leader)", g_ui.me);
  if (g_ui.lobby_opponent[0] != '\0')
    mvwprintw(stdscr, y + 4, x + 4, "2. %s (Opponent)", g_ui.lobby_opponent);
  else
    mvwprintw(stdscr, y + 4, x + 4, "2. (Waiting for opponent...)");

  for (int i = 0; i < g_ui.num_spectators && (5 + i) < box_h - 1; i++) {
    mvwprintw(stdscr, y + 5 + i, x + 4, "%d. %s (spectator)", i + 3,
              g_ui.lobby_spectators[i]);
  }

  /* Buttons below member box: Start, Invite, Close Lobby */
  int btn_y = y + box_h + 2;
  int bx = x + box_w / 2 - 8;
  const char *btns[3] = {"Start", "Invite", "Close Lobby"};
  for (int i = 0; i < 3; i++) {
    print_button(stdscr, btn_y + i * 2, bx, btns[i],
                 g_ui.focus == FOCUS_MAIN && g_ui.main_idx == i);
  }

  /* Checkbox on right: [X] Private? */
  char priv_label[24];
  snprintf(priv_label, sizeof(priv_label), "[%c] Private?",
           g_ui.is_private ? 'X' : ' ');
  print_button(stdscr, y + 3, rx + 1, priv_label, g_ui.focus == FOCUS_SIDE);
  (void)rw;
}

static void render_screen_lobby_member(int y, int x, int h, int w, int rx,
                                       int rw) {
  int box_w = w - 4;
  int box_h = h - 7;
  draw_box_style(stdscr, y + 1, x + 2, box_h, box_w, 0,
                 g_ui.focus == FOCUS_MAIN);

  wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(stdscr, y + 1, x + 4, " Lobby (%s) - Waiting for leader to start ",
            g_ui.lobby_leader);
  wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

  mvwprintw(stdscr, y + 3, x + 4, "1. %s (You)", g_ui.me);
  mvwprintw(stdscr, y + 4, x + 4, "2. %s (Opponent - Leader)",
            g_ui.lobby_leader);

  int line_idx = 5;
  if (g_ui.lobby_opponent[0] != '\0' &&
      strcmp(g_ui.lobby_opponent, g_ui.me) != 0) {
    mvwprintw(stdscr, y + line_idx++, x + 4, "3. %s (Seated Player)",
              g_ui.lobby_opponent);
  }
  for (int i = 0; i < g_ui.num_spectators && line_idx < box_h - 1; i++) {
    if (strcmp(g_ui.lobby_spectators[i], g_ui.me) == 0)
      continue;
    mvwprintw(stdscr, y + line_idx, x + 4, "%d. %s (spectator)", line_idx - 1,
              g_ui.lobby_spectators[i]);
    line_idx++;
  }

  print_button(stdscr, y + box_h + 2, x + box_w / 2 - 5, "Quit",
               g_ui.focus == FOCUS_MAIN);
  (void)rx;
  (void)rw;
}

static void render_game_layout(int rows, int cols) {
  int board_y = 2;
  int board_x = (cols - 66) / 2;
  if (board_x < 2)
    board_x = 2;

  if (g_ui.screen == SCREEN_REPLAY_VIEW) {
    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(
        stdscr, 1, board_x,
        "Replay Game #%d - Step %d/%d [%s] (Space: Play/Pause, <-/->: Step)",
        g_ui.replay_id, g_ui.replay_step, g_ui.replay_total,
        g_ui.replay_autoplay ? "Autoplaying" : "Paused");
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    ui_board_draw(stdscr, board_y, board_x, &g_ui.replay_board, PLAYER_0,
                  g_ui.replay_players[0], g_ui.replay_players[1], -1, 0);
  } else if (g_ui.screen == SCREEN_LOCAL_GAME) {
    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(stdscr, 1, board_x, "Local Game - %s", g_ui.local_info);
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    ui_board_draw(stdscr, board_y, board_x, &g_ui.local_board,
                  g_ui.local_board.turn, "Player 1", "Player 2",
                  g_ui.local_selected_pit,
                  g_ui.local_board.status == GAME_ONGOING);
  } else if (g_ui.screen == SCREEN_ONLINE_GAME) {
    Side viewer = (g_ui.my_side == PLAYER_1) ? PLAYER_1 : PLAYER_0;
    int interactive = g_ui.playing && (g_ui.my_side >= 0) &&
                      (g_ui.online_board.turn == (Side)g_ui.my_side);

    wattron(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(stdscr, 1, board_x, "Online Match (%s vs %s) - %s",
              g_ui.game_players[0], g_ui.game_players[1],
              g_ui.game_end_info[0] ? g_ui.game_end_info : g_ui.last_move_info);
    wattroff(stdscr, COLOR_PAIR(CP_TITLE) | A_BOLD);

    ui_board_draw(stdscr, board_y, board_x, &g_ui.online_board, viewer,
                  g_ui.game_players[0], g_ui.game_players[1], g_ui.selected_pit,
                  interactive);
  }

  /* Bottom-left Chat & Bottom-right Quit matching wireframe */
  int chat_y = board_y + 11;
  int chat_h = rows - chat_y - 1;
  if (chat_h < 6)
    chat_h = 6;
  int chat_w = cols / 2;
  if (chat_w > 46)
    chat_w = 46;

  render_chat_box(chat_y, 2, chat_h, chat_w);

  if (g_ui.screen == SCREEN_ONLINE_GAME && !g_ui.playing && g_ui.in_lobby) {
    print_button(stdscr, rows - 5, cols - 22, "Back to Lobby (L)",
                 g_ui.focus == FOCUS_SIDE && g_ui.side_idx == 0);
    print_button(stdscr, rows - 3, cols - 18, "Quit (Q)",
                 g_ui.focus == FOCUS_SIDE && g_ui.side_idx == 1);
  } else {
    print_button(stdscr, rows - 3, cols - 18, "Quit (Q)",
                 g_ui.focus == FOCUS_SIDE);
  }
}

void ui_render(void) {
  if (!g_ui.needs_redraw)
    return;
  g_ui.needs_redraw = 0;

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  werase(stdscr);

  if (rows < 22 || cols < 76) {
    mvwprintw(stdscr, rows / 2, 2,
              "Terminal too small (%dx%d). Please resize to at least 76x22.",
              cols, rows);
    wrefresh(stdscr);
    return;
  }

  /* Outer window border */
  draw_box_style(stdscr, 0, 0, rows, cols, 0, 0);

  if (g_ui.screen == SCREEN_REPLAY_VIEW || g_ui.screen == SCREEN_LOCAL_GAME ||
      g_ui.screen == SCREEN_ONLINE_GAME) {
    render_game_layout(rows, cols);
  } else {
    /* Standard 3-column wireframe layout:
     * Left: Chat (dashed box)
     * Center: Main Screen panel
     * Right: Action buttons + Invites (dashed box at bottom-right)
     */
    int chat_w = 26;
    int right_w = 22;
    int center_x = chat_w + 2;
    int center_w = cols - chat_w - right_w - 4;
    int right_x = cols - right_w - 1;

    render_chat_box(1, 1, rows - 2, chat_w);
    render_invites_box(rows - 10, right_x, 9, right_w);

    int main_h = rows - 2;
    switch (g_ui.screen) {
    case SCREEN_MAIN_MENU:
      render_screen_main_menu(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_PROFILE:
      render_screen_profile(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_FRIENDS:
      render_screen_friends(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_ONLINE_PLAYERS:
      render_screen_online_players(1, center_x, main_h, center_w, right_x,
                                   right_w);
      break;
    case SCREEN_RANKING:
      render_screen_ranking(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_REPLAYS:
      render_screen_replays(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_PLAY_MENU:
      render_screen_play_menu(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_JOIN_LOBBY:
      render_screen_join_lobby(1, center_x, main_h, center_w, right_x, right_w);
      break;
    case SCREEN_LOBBY_LEADER:
      render_screen_lobby_leader(1, center_x, main_h, center_w, right_x,
                                 right_w);
      break;
    case SCREEN_LOBBY_MEMBER:
      render_screen_lobby_member(1, center_x, main_h, center_w, right_x,
                                 right_w);
      break;
    default:
      break;
    }
  }

  /* Status message bar at the very bottom border */
  if (g_ui.status_msg[0] != '\0') {
    int pair = g_ui.status_is_err ? CP_ERROR : CP_SUCCESS;
    wattron(stdscr, COLOR_PAIR(pair) | A_BOLD);
    mvwprintw(stdscr, rows - 1, 3, " %s ", g_ui.status_msg);
    wattroff(stdscr, COLOR_PAIR(pair) | A_BOLD);
  }

  render_modal(rows, cols);
  wrefresh(stdscr);
}

/* Open the Bio Editor modal populated with current bio lines */
static void open_bio_editor(void) {
  memset(g_ui.bio_edit, 0, sizeof(g_ui.bio_edit));
  for (int i = 0; i < g_ui.bio_count && i < BIO_LINES; i++) {
    snprintf(g_ui.bio_edit[i], BIO_LINE_LEN + 1, "%s", g_ui.bio_lines[i]);
  }
  g_ui.bio_edit_line = 0;
  g_ui.bio_edit_col = (int)strlen(g_ui.bio_edit[0]);
  g_ui.modal = MODAL_EDIT_BIO;
  g_ui.focus = FOCUS_MODAL;
}

static void save_bio_editor(void) {
  int last_non_empty = -1;
  for (int i = 0; i < BIO_LINES; i++) {
    if (g_ui.bio_edit[i][0] != '\0')
      last_non_empty = i;
  }

  char payload[BIO_LEN + 32];
  payload[0] = '\0';
  for (int i = 0; i <= last_non_empty; i++) {
    if (i > 0)
      strcat(payload, "\\n");
    strcat(payload, g_ui.bio_edit[i]);
  }
  send_line(g_ui.sock, "/bio set %s", payload);
  g_ui.modal = MODAL_NONE;
  g_ui.focus = FOCUS_MAIN;
}

static void handle_modal_key(int ch) {
  if (g_ui.modal == MODAL_EDIT_BIO) {
    if (ch == 27) { /* Esc */
      g_ui.modal = MODAL_NONE;
      g_ui.focus = FOCUS_MAIN;
      return;
    }
    if (ch == 19 || ch == KEY_F(2) || ch == '\t') { /* Ctrl-S, F2, or Tab */
      save_bio_editor();
      return;
    }
    if (ch == KEY_UP && g_ui.bio_edit_line > 0) {
      g_ui.bio_edit_line--;
      g_ui.bio_edit_col = (int)strlen(g_ui.bio_edit[g_ui.bio_edit_line]);
    } else if ((ch == KEY_DOWN || ch == '\n' || ch == '\r') &&
               g_ui.bio_edit_line < BIO_LINES - 1) {
      g_ui.bio_edit_line++;
      g_ui.bio_edit_col = (int)strlen(g_ui.bio_edit[g_ui.bio_edit_line]);
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      char *line = g_ui.bio_edit[g_ui.bio_edit_line];
      int len = (int)strlen(line);
      if (len > 0)
        line[len - 1] = '\0';
    } else if (ch >= 32 && ch <= 126) {
      char *line = g_ui.bio_edit[g_ui.bio_edit_line];
      int len = (int)strlen(line);
      if (len < BIO_LINE_LEN) {
        line[len] = (char)ch;
        line[len + 1] = '\0';
      }
    }
    return;
  }

  if (g_ui.modal == MODAL_ADD_FRIEND || g_ui.modal == MODAL_INVITE_USER) {
    if (ch == 27) {
      g_ui.modal = MODAL_NONE;
      g_ui.focus = FOCUS_MAIN;
      return;
    }
    if (ch == '\n' || ch == '\r') {
      if (g_ui.modal_input[0] != '\0') {
        if (g_ui.modal == MODAL_ADD_FRIEND)
          send_line(g_ui.sock, "/friend %s", g_ui.modal_input);
        else
          send_line(g_ui.sock, "/invite %s", g_ui.modal_input);
      }
      g_ui.modal = MODAL_NONE;
      g_ui.focus = FOCUS_MAIN;
      return;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      int len = (int)strlen(g_ui.modal_input);
      if (len > 0)
        g_ui.modal_input[len - 1] = '\0';
    } else if (isalnum(ch) || ch == '_') {
      int len = (int)strlen(g_ui.modal_input);
      if (len < NAME_LEN) {
        g_ui.modal_input[len] = (char)ch;
        g_ui.modal_input[len + 1] = '\0';
      }
    }
    return;
  }

  if (g_ui.modal == MODAL_PENDING_INVITES) {
    int rem = ui_state_invite_remaining();
    int total = g_ui.num_friend_reqs_in + (rem > 0 ? 1 : 0);
    if (ch == 27 || ch == 'q' || ch == 'Q') {
      g_ui.modal = MODAL_NONE;
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_UP && g_ui.modal_idx > 0) {
      g_ui.modal_idx--;
    } else if (ch == KEY_DOWN && g_ui.modal_idx < total - 1) {
      g_ui.modal_idx++;
    } else if ((ch == 'a' || ch == 'A' || ch == '\n' || ch == '\r') &&
               total > 0) {
      if (g_ui.modal_idx < g_ui.num_friend_reqs_in) {
        send_line(g_ui.sock, "/friend %s", g_ui.friend_reqs_in[g_ui.modal_idx]);
      } else if (rem > 0) {
        g_ui.modal = MODAL_NONE;
        g_ui.focus = FOCUS_MAIN;
        send_line(g_ui.sock, "/accept");
      }
    } else if ((ch == 'd' || ch == 'D' || ch == 'x' || ch == 'X') &&
               total > 0) {
      if (g_ui.modal_idx < g_ui.num_friend_reqs_in) {
        send_line(g_ui.sock, "/unfriend %s",
                  g_ui.friend_reqs_in[g_ui.modal_idx]);
      } else if (rem > 0) {
        g_ui.modal = MODAL_NONE;
        g_ui.focus = FOCUS_MAIN;
        send_line(g_ui.sock, "/deny");
      }
    }
  }
}

static void handle_chat_key(int ch) {
  if (ch == '\t' || ch == 27) {
    g_ui.focus = FOCUS_MAIN;
    return;
  }
  if (ch == '\n' || ch == '\r') {
    if (g_ui.chat_input[0] != '\0') {
      if (g_ui.chat_input[0] == '/') {
        net_client_notify_chat_cmd(g_ui.chat_input);
        send_line(g_ui.sock, "%s", g_ui.chat_input);
      } else {
        send_line(g_ui.sock, "/say %s", g_ui.chat_input);
        ui_state_add_chat(CHAT_PUBLIC, g_ui.me, g_ui.chat_input);
      }
      g_ui.chat_input[0] = '\0';
      g_ui.chat_cursor = 0;
    }
    return;
  }
  if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
    int len = (int)strlen(g_ui.chat_input);
    if (len > 0)
      g_ui.chat_input[len - 1] = '\0';
  } else if (ch >= 32 && ch <= 126) {
    int len = (int)strlen(g_ui.chat_input);
    if (len < CHAT_LINE_LEN - 1) {
      g_ui.chat_input[len] = (char)ch;
      g_ui.chat_input[len + 1] = '\0';
    }
  }
}

static void play_local_house(int house_idx) {
  if (g_ui.local_board.status != GAME_ONGOING)
    return;
  Side turn = g_ui.local_board.turn;
  PlayResult res = execute_play(&g_ui.local_board, turn, house_idx);
  if (res == PLAY_OK) {
    snprintf(g_ui.local_info, sizeof(g_ui.local_info),
             "Player %d played house %d", turn + 1, house_idx + 1);
  } else if (res == PLAY_EMPTY_HOUSE) {
    snprintf(g_ui.local_info, sizeof(g_ui.local_info),
             "House %d is empty! Choose another.", house_idx + 1);
  } else if (res == PLAY_MUST_FEED) {
    snprintf(g_ui.local_info, sizeof(g_ui.local_info),
             "Must feed opponent! Choose a house that gives seeds.");
  }
}

void ui_handle_key(int ch) {
  g_ui.needs_redraw = 1;

  if (ch == KEY_RESIZE)
    return;

  /* Global shortcut for accepting/denying pending lobby or friend invites:
   * Ctrl-A (1) to accept, Ctrl-D (4) to deny */
  if (ch == 1) {
    if (ui_state_invite_remaining() > 0) {
      send_line(g_ui.sock, "/accept");
      return;
    } else if (g_ui.num_friend_reqs_in > 0) {
      send_line(g_ui.sock, "/friend %s", g_ui.friend_reqs_in[0]);
      return;
    }
  }
  if (ch == 4) {
    if (ui_state_invite_remaining() > 0) {
      send_line(g_ui.sock, "/deny");
      return;
    } else if (g_ui.num_friend_reqs_in > 0) {
      send_line(g_ui.sock, "/unfriend %s", g_ui.friend_reqs_in[0]);
      return;
    }
  }

  if (g_ui.modal != MODAL_NONE) {
    handle_modal_key(ch);
    return;
  }

  if (g_ui.focus == FOCUS_CHAT) {
    handle_chat_key(ch);
    return;
  }

  if (ch == '\t') {
    g_ui.focus = FOCUS_CHAT;
    return;
  }

  /* Mouse support for board pits */
  if (ch == KEY_MOUSE) {
    MEVENT ev;
    if (getmouse(&ev) == OK && (ev.bstate & BUTTON1_CLICKED)) {
      int rows, cols;
      getmaxyx(stdscr, rows, cols);
      (void)rows;
      int board_x = (cols - 66) / 2;
      if (board_x < 2)
        board_x = 2;
      int pit = ui_board_click_pit(2, board_x, ev.y, ev.x);
      if (pit >= 0) {
        if (g_ui.screen == SCREEN_LOCAL_GAME) {
          g_ui.local_selected_pit = pit;
          play_local_house(pit);
        } else if (g_ui.screen == SCREEN_ONLINE_GAME && g_ui.playing) {
          g_ui.selected_pit = pit;
          send_line(g_ui.sock, "/move %d", pit + 1);
        }
      }
    }
    return;
  }

  switch (g_ui.screen) {
  case SCREEN_MAIN_MENU:
    if (ch == KEY_LEFT)
      g_ui.focus = FOCUS_MAIN;
    else if (ch == KEY_RIGHT)
      g_ui.focus = FOCUS_SIDE;
    else if (ch == KEY_UP) {
      if (g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0)
        g_ui.main_idx--;
      else if (g_ui.focus == FOCUS_SIDE && g_ui.side_idx > 0)
        g_ui.side_idx--;
    } else if (ch == KEY_DOWN) {
      if (g_ui.focus == FOCUS_MAIN && g_ui.main_idx < 3)
        g_ui.main_idx++;
      else if (g_ui.focus == FOCUS_SIDE && g_ui.side_idx < 2)
        g_ui.side_idx++;
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_MAIN) {
        if (g_ui.main_idx == 0)
          ui_state_set_screen(SCREEN_PLAY_MENU);
        else if (g_ui.main_idx == 1)
          ui_state_set_screen(SCREEN_RANKING);
        else if (g_ui.main_idx == 2)
          ui_state_set_screen(SCREEN_REPLAYS);
        else if (g_ui.main_idx == 3) {
          send_line(g_ui.sock, "/quit");
          g_ui.running = 0;
        }
      } else if (g_ui.focus == FOCUS_SIDE) {
        if (g_ui.side_idx == 0)
          ui_state_set_screen(SCREEN_PROFILE);
        else if (g_ui.side_idx == 1)
          ui_state_set_screen(SCREEN_FRIENDS);
        else if (g_ui.side_idx == 2)
          ui_state_set_screen(SCREEN_ONLINE_PLAYERS);
      }
    } else if (ch == 'q' || ch == 'Q') {
      send_line(g_ui.sock, "/quit");
      g_ui.running = 0;
    }
    break;

  case SCREEN_PROFILE:
    if (ch == 27 || ch == 'b' || ch == 'B' || ch == 'q') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_LEFT || ch == KEY_UP) {
      g_ui.focus = FOCUS_MAIN;
      g_ui.main_idx = 0;
    } else if (ch == KEY_RIGHT || ch == KEY_DOWN) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == 'e' || ch == 'E') {
      open_bio_editor();
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE || g_ui.main_idx == 1)
        ui_state_set_screen(SCREEN_MAIN_MENU);
      else
        open_bio_editor();
    }
    break;

  case SCREEN_FRIENDS:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_LEFT) {
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_RIGHT) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == KEY_UP) {
      if (g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0)
        g_ui.main_idx--;
      else if (g_ui.focus == FOCUS_SIDE && g_ui.side_idx > 0)
        g_ui.side_idx--;
    } else if (ch == KEY_DOWN) {
      if (g_ui.focus == FOCUS_MAIN && g_ui.main_idx < g_ui.num_friends)
        g_ui.main_idx++;
      else if (g_ui.focus == FOCUS_SIDE && g_ui.side_idx < 1)
        g_ui.side_idx++;
    } else if (ch == 'a' || ch == 'A') {
      g_ui.modal_input[0] = '\0';
      g_ui.modal = MODAL_ADD_FRIEND;
      g_ui.focus = FOCUS_MODAL;
    } else if (ch == 'p' || ch == 'P') {
      g_ui.modal = MODAL_PENDING_INVITES;
      g_ui.focus = FOCUS_MODAL;
    } else if ((ch == 'x' || ch == 'X' || ch == KEY_DC) &&
               g_ui.focus == FOCUS_MAIN && g_ui.main_idx < g_ui.num_friends) {
      send_line(g_ui.sock, "/unfriend %s", g_ui.friends[g_ui.main_idx].name);
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_MAIN) {
        if (g_ui.main_idx == g_ui.num_friends) {
          g_ui.modal_input[0] = '\0';
          g_ui.modal = MODAL_ADD_FRIEND;
          g_ui.focus = FOCUS_MODAL;
        } else if (g_ui.main_idx < g_ui.num_friends) {
          send_line(g_ui.sock, "/unfriend %s",
                    g_ui.friends[g_ui.main_idx].name);
        }
      } else if (g_ui.focus == FOCUS_SIDE) {
        if (g_ui.side_idx == 0) {
          g_ui.modal = MODAL_PENDING_INVITES;
          g_ui.focus = FOCUS_MODAL;
        } else {
          ui_state_set_screen(SCREEN_MAIN_MENU);
        }
      }
    }
    break;

  case SCREEN_ONLINE_PLAYERS:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_LEFT) {
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_RIGHT) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == KEY_UP && g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0) {
      g_ui.main_idx--;
    } else if (ch == KEY_DOWN && g_ui.focus == FOCUS_MAIN &&
               g_ui.main_idx < g_ui.num_online - 1) {
      g_ui.main_idx++;
    } else if (ch == 'a' || ch == 'A' || ch == '\n' || ch == '\r' ||
               ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE) {
        ui_state_set_screen(SCREEN_MAIN_MENU);
      } else if (g_ui.main_idx < g_ui.num_online) {
        const char *target = g_ui.online_users[g_ui.main_idx].name;
        if (strcmp(target, g_ui.me) != 0)
          send_line(g_ui.sock, "/friend %s", target);
      }
    }
    break;

  case SCREEN_RANKING:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_LEFT) {
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_RIGHT) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == KEY_UP && g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0) {
      g_ui.main_idx--;
    } else if (ch == KEY_DOWN && g_ui.focus == FOCUS_MAIN &&
               g_ui.main_idx < g_ui.num_rankings - 1) {
      g_ui.main_idx++;
    } else if (ch == 'a' || ch == 'A' || ch == '\n' || ch == '\r' ||
               ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE) {
        ui_state_set_screen(SCREEN_MAIN_MENU);
      } else if (g_ui.main_idx < g_ui.num_rankings) {
        const char *target = g_ui.rankings[g_ui.main_idx].name;
        if (strcmp(target, g_ui.me) != 0)
          send_line(g_ui.sock, "/friend %s", target);
      }
    }
    break;

  case SCREEN_REPLAYS:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_LEFT) {
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_RIGHT) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == KEY_UP && g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0) {
      g_ui.main_idx--;
    } else if (ch == KEY_DOWN && g_ui.focus == FOCUS_MAIN &&
               g_ui.main_idx < g_ui.num_history - 1) {
      g_ui.main_idx++;
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE) {
        ui_state_set_screen(SCREEN_MAIN_MENU);
      } else if (g_ui.main_idx < g_ui.num_history) {
        int id = g_ui.history[g_ui.main_idx].id;
        g_ui.replay_id = id;
        g_ui.replay_step = 0;
        g_ui.replay_total = g_ui.history[g_ui.main_idx].moves;
        g_ui.replay_autoplay = 1;
        g_ui.replay_last_step_ms = ui_now_ms();
        ui_state_set_screen(SCREEN_REPLAY_VIEW);
        send_line(g_ui.sock, "/replay %d 0", id);
      }
    }
    break;

  case SCREEN_REPLAY_VIEW:
    if (ch == 27 || ch == 'q' || ch == 'Q' || ch == 'b' ||
        (g_ui.focus == FOCUS_SIDE && (ch == '\n' || ch == '\r'))) {
      g_ui.replay_autoplay = 0;
      ui_state_set_screen(SCREEN_REPLAYS);
    } else if (ch == ' ') {
      g_ui.replay_autoplay = !g_ui.replay_autoplay;
      g_ui.replay_last_step_ms = ui_now_ms();
    } else if (ch == KEY_LEFT && g_ui.replay_step > 0) {
      g_ui.replay_autoplay = 0;
      send_line(g_ui.sock, "/replay %d %d", g_ui.replay_id,
                g_ui.replay_step - 1);
    } else if (ch == KEY_RIGHT && g_ui.replay_step < g_ui.replay_total) {
      g_ui.replay_autoplay = 0;
      send_line(g_ui.sock, "/replay %d %d", g_ui.replay_id,
                g_ui.replay_step + 1);
    }
    break;

  case SCREEN_PLAY_MENU:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_MAIN_MENU);
    } else if (ch == KEY_UP && g_ui.main_idx > 0) {
      g_ui.main_idx--;
    } else if (ch == KEY_DOWN && g_ui.main_idx < 3) {
      g_ui.main_idx++;
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.main_idx == 0)
        ui_state_set_screen(SCREEN_LOCAL_GAME);
      else if (g_ui.main_idx == 1)
        send_line(g_ui.sock, "/create");
      else if (g_ui.main_idx == 2)
        ui_state_set_screen(SCREEN_JOIN_LOBBY);
      else if (g_ui.main_idx == 3)
        ui_state_set_screen(SCREEN_MAIN_MENU);
    }
    break;

  case SCREEN_LOCAL_GAME:
    if (ch == 27 || ch == 'q' || ch == 'Q') {
      ui_state_set_screen(SCREEN_PLAY_MENU);
    } else if (ch >= '1' && ch <= '6') {
      int pit = ch - '1';
      g_ui.local_selected_pit = pit;
      play_local_house(pit);
    } else if (ch == KEY_LEFT && g_ui.local_selected_pit > 0) {
      g_ui.local_selected_pit--;
    } else if (ch == KEY_RIGHT && g_ui.local_selected_pit < 5) {
      g_ui.local_selected_pit++;
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      play_local_house(g_ui.local_selected_pit);
    }
    break;

  case SCREEN_JOIN_LOBBY:
    if (ch == 27 || ch == 'b' || ch == 'B') {
      ui_state_set_screen(SCREEN_PLAY_MENU);
    } else if (ch == KEY_LEFT) {
      g_ui.focus = FOCUS_MAIN;
    } else if (ch == KEY_RIGHT) {
      g_ui.focus = FOCUS_SIDE;
    } else if (ch == KEY_UP && g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0) {
      g_ui.main_idx--;
    } else if (ch == KEY_DOWN && g_ui.focus == FOCUS_MAIN &&
               g_ui.main_idx < g_ui.num_lobbies - 1) {
      g_ui.main_idx++;
    } else if (ch == 'r' || ch == 'R') {
      send_line(g_ui.sock, "/lobbies");
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE) {
        ui_state_set_screen(SCREEN_PLAY_MENU);
      } else if (g_ui.main_idx < g_ui.num_lobbies) {
        send_line(g_ui.sock, "/join %s", g_ui.lobbies[g_ui.main_idx].leader);
      }
    }
    break;

  case SCREEN_LOBBY_LEADER:
    if (ch == KEY_LEFT)
      g_ui.focus = FOCUS_MAIN;
    else if (ch == KEY_RIGHT)
      g_ui.focus = FOCUS_SIDE;
    else if (ch == KEY_UP && g_ui.focus == FOCUS_MAIN && g_ui.main_idx > 0)
      g_ui.main_idx--;
    else if (ch == KEY_DOWN && g_ui.focus == FOCUS_MAIN && g_ui.main_idx < 2)
      g_ui.main_idx++;
    else if (ch == 'p' || ch == 'P') {
      net_client_toggle_lobby_privacy();
    } else if (ch == '\n' || ch == '\r' || ch == ' ') {
      if (g_ui.focus == FOCUS_SIDE) {
        net_client_toggle_lobby_privacy();
      } else if (g_ui.main_idx == 0) {
        send_line(g_ui.sock, "/start");
      } else if (g_ui.main_idx == 1) {
        g_ui.modal_input[0] = '\0';
        g_ui.modal = MODAL_INVITE_USER;
        g_ui.focus = FOCUS_MODAL;
      } else if (g_ui.main_idx == 2) {
        send_line(g_ui.sock, "/leave");
        ui_state_set_screen(SCREEN_PLAY_MENU);
      }
    } else if (ch == 27 || ch == 'q' || ch == 'Q') {
      send_line(g_ui.sock, "/leave");
      ui_state_set_screen(SCREEN_PLAY_MENU);
    }
    break;

  case SCREEN_LOBBY_MEMBER:
    if (ch == 27 || ch == 'q' || ch == 'Q' || ch == '\n' || ch == '\r' ||
        ch == ' ') {
      send_line(g_ui.sock, "/leave");
      ui_state_set_screen(SCREEN_JOIN_LOBBY);
    }
    break;

  case SCREEN_ONLINE_GAME:
    if (ch == 27 || ch == 'q' || ch == 'Q') {
      if (g_ui.in_lobby)
        send_line(g_ui.sock, "/leave");
      ui_state_set_screen(SCREEN_PLAY_MENU);
    } else if ((ch == 'l' || ch == 'L') && !g_ui.playing && g_ui.in_lobby) {
      ui_state_set_screen(g_ui.is_leader ? SCREEN_LOBBY_LEADER
                                         : SCREEN_LOBBY_MEMBER);
    } else if (g_ui.playing && g_ui.my_side >= 0) {
      if (ch >= '1' && ch <= '6') {
        int pit = ch - '1';
        g_ui.selected_pit = pit;
        send_line(g_ui.sock, "/move %d", pit + 1);
      } else if (ch == KEY_LEFT && g_ui.selected_pit > 0) {
        g_ui.selected_pit--;
      } else if (ch == KEY_RIGHT && g_ui.selected_pit < 5) {
        g_ui.selected_pit++;
      } else if (ch == '\n' || ch == '\r' || ch == ' ') {
        send_line(g_ui.sock, "/move %d", g_ui.selected_pit + 1);
      }
    }
    break;
  }
}
