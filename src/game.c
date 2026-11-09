#include "game.h"
#include <stdio.h>

int abs_index(Side side, int house) {
  return (side == PLAYER_0) ? house : house + HOUSES_PER_SIDE;
}

// initializes the board with the standart initial board values
void init_board(Board *board, Side first_player) {
  for (int i = 0; i < NUM_HOUSES; i++)
    board->houses[i] = INITIAL_SEEDS;
  board->captured[0] = board->captured[1] = 0;
  board->turn = first_player;
  board->idle_moves = 0;
  board->status = GAME_ONGOING;
  board->winner = WINNER_NONE;
}

// returns the side of the opponent player, relative to the function input
// parameter
Side opponent(Side side) { return (side == PLAYER_0) ? PLAYER_1 : PLAYER_0; }

// calculates the total number of seeds for a determined player (side on input
// parameter)
static int side_seeds(const Board *board, Side side) {
  int total = 0;
  for (int i = 0; i < HOUSES_PER_SIDE; i++)
    total += board->houses[abs_index(side, i)];
  return total;
}

// distributes seeds from one house to the next ones, in a counter-clockwise
// direction, and returns the index of the last sowed house
static int sow(Board *board, int start_index) {
  int seeds = board->houses[start_index];
  int curr_index = start_index;
  board->houses[start_index] = 0;

  while (seeds > 0) {
    curr_index = (curr_index + 1) % NUM_HOUSES;
    if (curr_index == start_index)
      continue;
    board->houses[curr_index]++;
    seeds--;
  }

  return curr_index;
}

// checks the validity of a play
static PlayResult check_move_rules(const Board *board, Side side,
                                   int house_id) {
  if (board->houses[abs_index(side, house_id)] <= 0)
    return PLAY_EMPTY_HOUSE;

  if (side_seeds(board, opponent(side)) == 0) {
    Board copy = *board;
    sow(&copy, abs_index(side, house_id));
    if (side_seeds(&copy, opponent(side)) == 0)
      return PLAY_MUST_FEED;
  }

  return PLAY_OK;
}

// verifies if it's possible to capture opponent's seeds, and, in case yes,
// captures them
static int capture(Board *board, Side side, int last_index) {
  int first = abs_index(opponent(side), 0);
  int last = first + HOUSES_PER_SIDE - 1;
  int stop = last_index;
  int total = 0;

  // verifies if the distribution ended up in the current player's side
  if (last_index < first || last_index > last)
    return 0;

  // defines until what house there will be captured seeds
  while (stop >= first &&
         (board->houses[stop] == 2 || board->houses[stop] == 3)) {
    stop--;
  }

  // counts all the captured seeds within the found interval
  for (int i = last_index; i > stop; i--)
    total += board->houses[i];
  // if the captured seeds equal to the total seeds of the opponent: abort
  // (grand slam rule)
  if (total == side_seeds(board, opponent(side)))
    return 0;
  // take away opponent's seeds
  for (int i = last_index; i > stop; i--)
    board->houses[i] = 0;

  return total;
}

// verifies if the given input player can make a move in the given moment the
// function is called and if it respects awale rules (check_move_rules function
// call)
PlayResult check_play(const Board *board, Side side, int house_id) {
  if (board->status == GAME_FINISHED)
    return PLAY_GAME_OVER;
  if (board->turn != side)
    return PLAY_NOT_YOUR_TURN;
  if (house_id < 0 || house_id >= HOUSES_PER_SIDE)
    return PLAY_INVALID_HOUSE;
  return check_move_rules(board, side, house_id);
}

// identifies and returns the number of all valid plays the given player can
// execute in the board
int get_valid_plays(const Board *board, Side side, int out[HOUSES_PER_SIDE]) {
  int count = 0;
  for (int house = 0; house < HOUSES_PER_SIDE; house++) {
    if (check_move_rules(board, side, house) == PLAY_OK)
      out[count++] = house;
  }
  return count;
}

// verifies if a match is over and, if yes, decides who's the winner
static void update_game_end(Board *board) {
  int valid[HOUSES_PER_SIDE];
  // boolean variable in order to know if a player has already the winning score
  // (25)
  int points_win = board->captured[PLAYER_0] >= WIN_SCORE ||
                   board->captured[PLAYER_1] >= WIN_SCORE;
  // boolean variable in order to know if the game is stalled
  int stalled = board->idle_moves >= MAX_IDLE_MOVES;

  if (!points_win) {
    // if the game is not stalled and there are still valid plays, continue the
    // game
    if (!stalled && get_valid_plays(board, board->turn, valid) > 0)
      return;
    // otherwise, count the number of captured seeds + house seeds each side has
    // and finishes the game
    for (Side s = PLAYER_0; s <= PLAYER_1; s++)
      board->captured[s] += side_seeds(board, s);
    for (int i = 0; i < NUM_HOUSES; i++)
      board->houses[i] = 0;
  }

  board->status = GAME_FINISHED;

  // decides the winner of the game
  if (board->captured[PLAYER_0] > board->captured[PLAYER_1])
    board->winner = WINNER_PLAYER_0;
  else if (board->captured[PLAYER_1] > board->captured[PLAYER_0])
    board->winner = WINNER_PLAYER_1;
  else
    board->winner = WINNER_DRAW;
}

// executes a play for the given side in the board and update game's status
PlayResult execute_play(Board *board, Side side, int house_id) {
  PlayResult check = check_play(board, side, house_id);
  if (check != PLAY_OK)
    return check;

  int last_index = sow(board, abs_index(side, house_id));
  int gained = capture(board, side, last_index);
  board->captured[side] += gained;
  board->idle_moves = (gained > 0) ? 0 : board->idle_moves + 1;
  board->turn = opponent(side);
  update_game_end(board);

  return check;
}

// ends imediatly the game for the player who forfeit and declares victory for
// the opponent
int forfeit(Board *board, Side loser) {
  if (loser != PLAYER_0 && loser != PLAYER_1)
    return -1;
  if (board->status == GAME_FINISHED)
    return -1;

  board->status = GAME_FINISHED;
  board->winner =
      (opponent(loser) == PLAYER_0) ? WINNER_PLAYER_0 : WINNER_PLAYER_1;
  return 0;
}

// initializes a new game
void init_game(Game *game, Side first_player) {
  init_board(&game->board, first_player);
  game->first_player = first_player;
  game->num_moves = 0;
}

// executes a play and saves history of valid chosen houses
PlayResult play_move(Game *game, Side side, int house_id) {
  PlayResult result = execute_play(&game->board, side, house_id);
  if (result == PLAY_OK)
    game->moves[game->num_moves++] = house_id;
  return result;
}

// re-runs a game based on the play history
int replay_game(const Game *game, int upto, Board *out) {
  init_board(out, game->first_player);
  for (int i = 0; i < upto && i < game->num_moves; i++) {
    if (execute_play(out, out->turn, game->moves[i]) != PLAY_OK)
      return -1;
  }
  if (upto >= game->num_moves && game->board.status == GAME_FINISHED) {

    out->status = GAME_FINISHED;
    out->winner = game->board.winner;
  }
  return 0;
}

// saves a game in a file in the format: first_player num_moves move_1 move_2
// ... move_N status winner
int save_game(const Game *game, const char *path) {
  FILE *file = fopen(path, "w");
  if (file == NULL)
    return -1;

  fprintf(file, "%d %d", game->first_player, game->num_moves);

  for (int i = 0; i < game->num_moves; i++)
    fprintf(file, " %d", game->moves[i]);

  fprintf(file, " %d %d\n", game->board.status, game->board.winner);

  fclose(file);
  return 0;
}

// parses through the file and saves in the game history if everything is
// validated
static int read_game(FILE *file, Game *game) {
  int first, count, house, status, winner;

  if (fscanf(file, "%d %d", &first, &count) != 2)
    return -1;
  if ((first != 0 && first != 1) || count < 0 || count > MAX_MOVES)
    return -1;

  init_game(game, (Side)first);
  for (int i = 0; i < count; i++) {
    if (fscanf(file, "%d", &house) != 1)
      return -1;
    if (play_move(game, game->board.turn, house) != PLAY_OK)
      return -1;
  }

  if (fscanf(file, "%d %d", &status, &winner) != 2)
    return -1;
  if (status != GAME_ONGOING && status != GAME_FINISHED)
    return -1;
  if (winner < WINNER_NONE || winner > WINNER_DRAW)
    return -1;

  if (status == GAME_FINISHED && game->board.status == GAME_ONGOING) {
    if (winner != WINNER_PLAYER_0 && winner != WINNER_PLAYER_1)
      return -1;
    game->board.status = GAME_FINISHED;
    game->board.winner = (Winner)winner;
  }

  return 0;
}

// loads the game from the specified file in the game struct history
int load_game(Game *game, const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL)
    return -1;

  int result = read_game(file, game);
  fclose(file);
  return result;
}
