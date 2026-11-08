#ifndef BOARD_VIEW_H
#define BOARD_VIEW_H

#include "game.h"
#include <stddef.h>

#define BOARD_VIEW_MAX 512

int render_board(const Board *board, Side viewer, const char *names[2],
                 char *buf, size_t size);

#endif
