#include "ui_board.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static void draw_rect(WINDOW *win, int y, int x, int h, int w) {
  mvwhline(win, y, x + 1, ACS_HLINE, w - 2);
  mvwhline(win, y + h - 1, x + 1, ACS_HLINE, w - 2);
  mvwvline(win, y + 1, x, ACS_VLINE, h - 2);
  mvwvline(win, y + 1, x + w - 1, ACS_VLINE, h - 2);
  mvwaddch(win, y, x, ACS_ULCORNER);
  mvwaddch(win, y, x + w - 1, ACS_URCORNER);
  mvwaddch(win, y + h - 1, x, ACS_LLCORNER);
  mvwaddch(win, y + h - 1, x + w - 1, ACS_LRCORNER);
}

void ui_board_draw(WINDOW *win, int start_y, int start_x, const Board *board,
                   Side viewer, const char *p0_name, const char *p1_name,
                   int selected_pit, int interactive) {
  const char *names[2] = {
      (p0_name && p0_name[0]) ? p0_name : "Player 1",
      (p1_name && p1_name[0]) ? p1_name : "Player 2",
  };
  Side top_side = opponent(viewer);
  Side bot_side = viewer;

  /* Layout geometry:
   * Left store:  x = start_x,      w = 12, h = 9
   * Center pits: x = start_x + 13, w = 40, h = 9
   * Right store: x = start_x + 54, w = 12, h = 9
   */
  int left_x = start_x;
  int mid_x = start_x + 13;
  int right_x = start_x + 54;
  int box_y = start_y + 1;

  /* Labels above stores matching wireframe ("player 1 points", "player 2
   * points") */
  wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
  mvwprintw(win, start_y, left_x, "%-12.12s", names[top_side]);
  mvwprintw(win, start_y, right_x, "%-12.12s", names[bot_side]);
  wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);

  /* Left store (Opponent / Top player points) */
  wattron(win, COLOR_PAIR(CP_STORE));
  draw_rect(win, box_y, left_x, 8, 12);
  mvwprintw(win, box_y + 2, left_x + 2, "points:");
  wattron(win, A_BOLD);
  mvwprintw(win, box_y + 4, left_x + 3, "[ %2d ]", board->captured[top_side]);
  wattroff(win, A_BOLD);
  wattroff(win, COLOR_PAIR(CP_STORE));

  /* Right store (Viewer / Bottom player points) */
  wattron(win, COLOR_PAIR(CP_STORE));
  draw_rect(win, box_y, right_x, 8, 12);
  mvwprintw(win, box_y + 2, right_x + 2, "points:");
  wattron(win, A_BOLD);
  mvwprintw(win, box_y + 4, right_x + 3, "[ %2d ]", board->captured[bot_side]);
  wattroff(win, A_BOLD);
  wattroff(win, COLOR_PAIR(CP_STORE));

  /* Center board frame */
  wattron(win, COLOR_PAIR(CP_BORDER));
  draw_rect(win, box_y, mid_x, 8, 40);
  wattroff(win, COLOR_PAIR(CP_BORDER));

  /* Top row of 6 pits (opponent's houses, rendered right-to-left: 5..0) */
  for (int i = 0; i < HOUSES_PER_SIDE; i++) {
    int house_idx = HOUSES_PER_SIDE - 1 - i;
    int seeds = board->houses[abs_index(top_side, house_idx)];
    int px = mid_x + 2 + i * 6;
    int py = box_y + 2;

    wattron(win, COLOR_PAIR(CP_PIT));
    mvwprintw(win, py, px, "(%2d )", seeds);
    wattroff(win, COLOR_PAIR(CP_PIT));
  }

  /* Bottom row of 6 pits (viewer's houses, rendered left-to-right: 0..5) */
  for (int i = 0; i < HOUSES_PER_SIDE; i++) {
    int seeds = board->houses[abs_index(bot_side, i)];
    int px = mid_x + 2 + i * 6;
    int py = box_y + 5;
    int is_sel = interactive && (selected_pit == i);

    if (is_sel) {
      wattron(win, COLOR_PAIR(CP_SELECTED) | A_BOLD);
      mvwprintw(win, py, px, "(%2d )", seeds);
      wattroff(win, COLOR_PAIR(CP_SELECTED) | A_BOLD);
    } else {
      wattron(win, COLOR_PAIR(CP_PIT_ACTIVE) | A_BOLD);
      mvwprintw(win, py, px, "(%2d )", seeds);
      wattroff(win, COLOR_PAIR(CP_PIT_ACTIVE) | A_BOLD);
    }

    /* Pit key number 1..6 below bottom row */
    mvwprintw(win, box_y + 6, px + 2, "%d", i + 1);
  }

  /* Turn / Winner indicator below board */
  int info_y = box_y + 8;
  if (board->status == GAME_FINISHED) {
    wattron(win, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
    if (board->winner == WINNER_DRAW) {
      mvwprintw(win, info_y, mid_x + 2, "Game Over: Draw!");
    } else {
      Side w = (board->winner == WINNER_PLAYER_0) ? PLAYER_0 : PLAYER_1;
      mvwprintw(win, info_y, mid_x + 2, "Winner: %s!", names[w]);
    }
    wattroff(win, COLOR_PAIR(CP_SUCCESS) | A_BOLD);
  } else {
    int my_turn = interactive && (board->turn == viewer);
    wattron(win, COLOR_PAIR(my_turn ? CP_SUCCESS : CP_TITLE) | A_BOLD);
    mvwprintw(win, info_y, mid_x + 2, "Turn: %s %s", names[board->turn],
              my_turn ? "(Your move: 1-6 or Arrows+Enter)" : "");
    wattroff(win, COLOR_PAIR(my_turn ? CP_SUCCESS : CP_TITLE) | A_BOLD);
  }
}

int ui_board_click_pit(int start_y, int start_x, int click_y, int click_x) {
  int mid_x = start_x + 13;
  int box_y = start_y + 1;
  int py = box_y + 5;

  if (click_y < py || click_y > py + 1)
    return -1;

  for (int i = 0; i < HOUSES_PER_SIDE; i++) {
    int px = mid_x + 2 + i * 6;
    if (click_x >= px && click_x < px + 5)
      return i;
  }
  return -1;
}
