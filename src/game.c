#include "game.h"
#include <stdio.h>

int abs_index(Side side, int house){
	return (side==PLAYER_0) ? house : house+HOUSES_PER_SIDE;
}

void init_board(Board *board, Side first_player){
	for (int i = 0; i<NUM_HOUSES; i++) board->houses[i] = INITIAL_SEEDS;
	board->captured[0] = board->captured[1] = 0;
	board->turn = first_player;
	board->idle_moves = 0;
	board->status = GAME_ONGOING;
	board->winner = WINNER_NONE;}


Side opponent(Side side){
	return (side==PLAYER_0) ? PLAYER_1 : PLAYER_0;
}

static int side_seeds(const Board *board, Side side){
	int total = 0;
	for (int i = 0; i<HOUSES_PER_SIDE; i++) total += board->houses[abs_index(side, i)];
	return total;
}


static int sow(Board *board, int start_index){
	int seeds = board->houses[start_index];
	int curr_index = start_index;
	board->houses[start_index] = 0;

	while (seeds > 0){
		curr_index = (curr_index+1)%NUM_HOUSES;
		if (curr_index == start_index) continue;
		board->houses[curr_index]++;
		seeds--;}

	return curr_index;
}


static PlayResult check_move_rules(const Board *board, Side side, int house_id){
	if (board->houses[abs_index(side, house_id)] <= 0) return PLAY_EMPTY_HOUSE;

	if (side_seeds(board, opponent(side)) == 0){
		Board copy = *board;
		sow(&copy, abs_index(side, house_id));
		if (side_seeds(&copy, opponent(side)) == 0) return PLAY_MUST_FEED;}

	return PLAY_OK;
}


static int capture(Board *board, Side side, int last_index){
	int first = abs_index(opponent(side), 0);
	int last = first+HOUSES_PER_SIDE-1;
	int total = 0;

	if (last_index < first || last_index > last) return 0;

	for (int i = last_index; i>=first; i--){
		if (board->houses[i] != 2 && board->houses[i] != 3) break;
		total += board->houses[i];
		board->houses[i] = 0;}

	return total;
}

PlayResult check_play(const Board *board, Side side, int house_id){
	if (board->status == GAME_FINISHED) return PLAY_GAME_OVER;
	if (board->turn != side) return PLAY_NOT_YOUR_TURN;
	if (house_id < 0 || house_id >= HOUSES_PER_SIDE) return PLAY_INVALID_HOUSE;
	return check_move_rules(board, side, house_id);
}

int get_valid_plays(const Board *board, Side side, int out[HOUSES_PER_SIDE]){
	int count = 0;
	for (int house = 0; house<HOUSES_PER_SIDE; house++){
		if (check_move_rules(board, side, house) == PLAY_OK) out[count++] = house;}
	return count;
}


static void update_game_end(Board *board){
	int valid[HOUSES_PER_SIDE];
	int points_win = board->captured[PLAYER_0] >= WIN_SCORE || board->captured[PLAYER_1] >= WIN_SCORE;
	int stalled = board->idle_moves >= MAX_IDLE_MOVES;

	if (!points_win){
		if (!stalled && get_valid_plays(board, board->turn, valid) > 0) return;

		for (Side s = PLAYER_0; s<=PLAYER_1; s++) board->captured[s] += side_seeds(board, s);
		for (int i = 0; i<NUM_HOUSES; i++) board->houses[i] = 0;}

	board->status = GAME_FINISHED;

	if (board->captured[PLAYER_0] > board->captured[PLAYER_1]) board->winner = WINNER_PLAYER_0;
	else if (board->captured[PLAYER_1] > board->captured[PLAYER_0]) board->winner = WINNER_PLAYER_1;
	else board->winner = WINNER_DRAW;
}


PlayResult execute_play(Board *board, Side side, int house_id){
	PlayResult check = check_play(board, side, house_id);
	if (check != PLAY_OK) return check;

	int last_index = sow(board, abs_index(side, house_id));
	int gained = capture(board, side, last_index);
	board->captured[side] += gained;
	board->idle_moves = (gained > 0) ? 0 : board->idle_moves+1;
	board->turn = opponent(side);
	update_game_end(board);	

	return check;
}

int forfeit(Board *board, Side loser){
	if (loser != PLAYER_0 && loser != PLAYER_1) return -1;
	if (board->status == GAME_FINISHED) return -1;

	board->status = GAME_FINISHED;
	board->winner = (opponent(loser) == PLAYER_0) ? WINNER_PLAYER_0 : WINNER_PLAYER_1;
	return 0;
}

void init_game(Game *game, Side first_player){
	init_board(&game->board, first_player);
	game->first_player = first_player;
	game->num_moves = 0;}

PlayResult play_move(Game *game, Side side, int house_id){
	PlayResult result = execute_play(&game->board, side, house_id);
	if (result == PLAY_OK) game->moves[game->num_moves++] = house_id;
	return result;
}

int replay_game(const Game *game, int upto, Board *out){
	init_board(out, game->first_player);
	for (int i = 0; i<upto && i<game->num_moves; i++){
		if (execute_play(out, out->turn, game->moves[i]) != PLAY_OK) return -1;}
	return 0;
}

int save_game(const Game *game, const char *path){
	FILE *file = fopen(path, "w");
	if (file == NULL) return -1;

	fprintf(file, "%d", game->first_player);
	for (int i = 0; i<game->num_moves; i++) fprintf(file, " %d", game->moves[i]);
	fprintf(file, "\n");

	fclose(file);
	return 0;
}

int load_game(Game *game, const char *path){
	FILE *file = fopen(path, "r");
	int first, house, scanned;
	if (file == NULL) return -1;

	if (fscanf(file, "%d", &first) != 1 || (first != 0 && first != 1)){
		fclose(file);
		return -1;}

	init_game(game, (Side)first);
	while ((scanned = fscanf(file, "%d", &house)) == 1){
		if (play_move(game, game->board.turn, house) != PLAY_OK){
			fclose(file);
			return -1;}}

	fclose(file);
	return (scanned == EOF) ? 0 : -1;
}

