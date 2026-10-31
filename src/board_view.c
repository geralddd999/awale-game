#include "board_view.h"
#include <stdarg.h>
#include <stdio.h>

static void append(char *buf, size_t size, size_t *len, const char *fmt, ...){
	va_list args;
	if (*len >= size) return;

	va_start(args, fmt);
	int written = vsnprintf(buf+*len, size-*len, fmt, args);
	va_end(args);

	if (written < 0) return;
	*len += written;
	if (*len >= size) *len = size-1;}

int render_board(const Board *board, Side viewer, const char *names[2], char *buf, size_t size){
	const char *default_names[2] = {"Player 0", "Player 1"};
	const char **label = (names != NULL) ? names : default_names;
	Side other = opponent(viewer);
	size_t len = 0;

	if (size == 0) return 0;
	buf[0] = '\0';

	append(buf, size, &len, "%s (%d captured)\n", label[other], board->captured[other]);

	for (int i = HOUSES_PER_SIDE-1; i>=0; i--)
		append(buf, size, &len, "[%2d]", board->houses[abs_index(other, i)]);
	append(buf, size, &len, "\n");

	for (int i = 0; i<HOUSES_PER_SIDE; i++)
		append(buf, size, &len, "[%2d]", board->houses[abs_index(viewer, i)]);
	append(buf, size, &len, "\n");

	for (int i = 0; i<HOUSES_PER_SIDE; i++)
		append(buf, size, &len, "  %d ", i);
	append(buf, size, &len, "\n");

	append(buf, size, &len, "%s (%d captured)\n", label[viewer], board->captured[viewer]);

	if (board->status == GAME_FINISHED){
		if (board->winner == WINNER_DRAW) append(buf, size, &len, "Game over: draw\n");
		else append(buf, size, &len, "Game over: %s winner\n", label[(board->winner == WINNER_PLAYER_0) ? PLAYER_0 : PLAYER_1]);}
	else append(buf, size, &len, "Turn: %s\n", label[board->turn]);

	return (int)len;
}
