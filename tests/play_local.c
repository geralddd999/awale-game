#include "board_view.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
  Board board;
  char buf[BOARD_VIEW_MAX];
  const char *names[2] = {"Player 0", "Player 1"};

  srand(time(NULL));
  init_board(&board, (Side)(rand() % 2));

  while (board.status == GAME_ONGOING) {
    render_board(&board, board.turn, names, buf, sizeof(buf));
    printf("\n%sHouse (1...6): ", buf);

    int house;
    if (scanf("%d", &house) != 1 || house == 0)
      break;

    PlayResult result = execute_play(&board, board.turn, house - 1);
    if (result != PLAY_OK)
      printf("Player rejected (code %d)\n", result);
  }

  render_board(&board, PLAYER_0, names, buf, sizeof(buf));
  printf("\n%s", buf);
  return 0;
}
