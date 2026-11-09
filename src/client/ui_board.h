#ifndef UI_BOARD_H
#define UI_BOARD_H

#include "game.h"
#include <ncurses.h>

void ui_board_draw(WINDOW *win, int start_y, int start_x, const Board *board,
                   Side viewer, const char *p0_name, const char *p1_name,
                   int selected_pit, int interactive);

int ui_board_click_pit(int start_y, int start_x, int click_y, int click_x);

#endif
