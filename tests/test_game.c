#include "game.h"
#include <stdio.h>
#include <string.h>

#define TMP_FILE "test_history.tmp"

static int passed = 0;
static int failed = 0;

#define CHECK(cond) do { \
	if (cond) passed++; \
	else { failed++; printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void empty_board(Board *board, Side turn){
	memset(board, 0, sizeof(Board));
	board->turn = turn;
	board->status = GAME_ONGOING;
	board->winner = WINNER_NONE;
}

static int total_seeds(const Board *board){
	int total = board->captured[0] + board->captured[1];
	for (int i = 0; i<NUM_HOUSES; i++) total += board->houses[i];
	return total;
}

static void test_init(void){
	Board board;
	init_board(&board, PLAYER_1);

	CHECK(board.turn == PLAYER_1);
	CHECK(board.status == GAME_ONGOING);
	CHECK(board.winner == WINNER_NONE);
	CHECK(board.captured[0] == 0 && board.captured[1] == 0);
	CHECK(board.idle_moves == 0);
	CHECK(total_seeds(&board) == 48);
	for (int i = 0; i<NUM_HOUSES; i++) CHECK(board.houses[i] == INITIAL_SEEDS);
}

static void test_validation(void){
	Board board;
	init_board(&board, PLAYER_0);

	CHECK(execute_play(&board, PLAYER_1, 0) == PLAY_NOT_YOUR_TURN);
	CHECK(execute_play(&board, PLAYER_0, 6) == PLAY_INVALID_HOUSE);
	CHECK(execute_play(&board, PLAYER_0, -1) == PLAY_INVALID_HOUSE);
	CHECK(total_seeds(&board) == 48 && board.turn == PLAYER_0);

	empty_board(&board, PLAYER_0);
	board.houses[1] = 1;
	board.houses[7] = 1;
	CHECK(check_play(&board, PLAYER_0, 0) == PLAY_EMPTY_HOUSE);
	CHECK(check_play(&board, PLAYER_0, 1) == PLAY_OK);
}

static void test_turn_change(void){
	Board board;
	init_board(&board, PLAYER_0);

	CHECK(execute_play(&board, PLAYER_0, 2) == PLAY_OK);
	CHECK(board.turn == PLAYER_1);
	CHECK(execute_play(&board, PLAYER_0, 2) == PLAY_NOT_YOUR_TURN);
	CHECK(execute_play(&board, PLAYER_1, 2) == PLAY_OK);
	CHECK(board.turn == PLAYER_0);
}

static void test_sowing(void){
	Board board;
	init_board(&board, PLAYER_0);
	execute_play(&board, PLAYER_0, 2);

	CHECK(board.houses[2] == 0);
	CHECK(board.houses[3] == 5 && board.houses[4] == 5 && board.houses[5] == 5 && board.houses[6] == 5);
	CHECK(board.houses[7] == 4);

	init_board(&board, PLAYER_0);
	board.houses[0] = 12;
	execute_play(&board, PLAYER_0, 0);

	CHECK(board.houses[0] == 0);
	CHECK(board.houses[1] == 6 && board.houses[2] == 5 && board.houses[11] == 5);
	CHECK(total_seeds(&board) == 56);
}

static void test_simple_capture(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.houses[5] = 1;
	board.houses[6] = 1;
	board.houses[7] = 5;

	CHECK(execute_play(&board, PLAYER_0, 5) == PLAY_OK);
	CHECK(board.captured[0] == 2);
	CHECK(board.houses[6] == 0);
	CHECK(board.turn == PLAYER_1);
	CHECK(board.status == GAME_ONGOING);
}

static void test_chain_capture(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.houses[5] = 2;
	board.houses[6] = 1;
	board.houses[7] = 2;
	board.houses[11] = 4;
	execute_play(&board, PLAYER_0, 5);

	CHECK(board.captured[0] == 5);
	CHECK(board.houses[6] == 0 && board.houses[7] == 0);
	CHECK(board.houses[11] == 4);
}

static void test_chain_stops(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.houses[5] = 2;
	board.houses[6] = 3;
	board.houses[7] = 1;
	board.houses[11] = 4;
	execute_play(&board, PLAYER_0, 5);

	CHECK(board.captured[0] == 2);
	CHECK(board.houses[7] == 0);
	CHECK(board.houses[6] == 4);
}

static void test_no_capture_own_side(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.houses[0] = 2;
	board.houses[1] = 1;
	board.houses[2] = 1;
	board.houses[8] = 5;
	execute_play(&board, PLAYER_0, 0);

	CHECK(board.captured[0] == 0);
	CHECK(board.houses[2] == 2);
}

static void test_player1_capture(void){
	Board board;
	empty_board(&board, PLAYER_1);
	board.houses[11] = 1;
	board.houses[0] = 1;
	board.houses[3] = 5;
	execute_play(&board, PLAYER_1, 5);

	CHECK(board.captured[1] == 2);
	CHECK(board.houses[0] == 0);
	CHECK(board.turn == PLAYER_0);
}

static void test_feed_rule(void){
	Board board;
	int out[HOUSES_PER_SIDE];
	empty_board(&board, PLAYER_0);
	board.houses[0] = 1;
	board.houses[5] = 1;

	CHECK(check_play(&board, PLAYER_0, 0) == PLAY_MUST_FEED);
	CHECK(check_play(&board, PLAYER_0, 5) == PLAY_OK);
	CHECK(get_valid_plays(&board, PLAYER_0, out) == 1);
	CHECK(out[0] == 5);
}

static void test_valid_plays(void){
	Board board;
	int out[HOUSES_PER_SIDE];
	init_board(&board, PLAYER_0);

	CHECK(get_valid_plays(&board, PLAYER_0, out) == 6);
	for (int i = 0; i<6; i++) CHECK(out[i] == i);

	board.houses[1] = 0;
	board.houses[4] = 0;
	CHECK(get_valid_plays(&board, PLAYER_0, out) == 4);
	CHECK(out[0] == 0 && out[1] == 2 && out[2] == 3 && out[3] == 5);
}

static void test_end_cannot_feed(void){
	Board board;
	empty_board(&board, PLAYER_1);
	board.houses[11] = 1;
	board.houses[1] = 1;
	board.houses[2] = 1;
	execute_play(&board, PLAYER_1, 5);

	CHECK(board.status == GAME_FINISHED);
	CHECK(board.winner == WINNER_PLAYER_0);
	CHECK(board.captured[0] == 3 && board.captured[1] == 0);
	CHECK(total_seeds(&board) == 3);
	CHECK(execute_play(&board, PLAYER_0, 1) == PLAY_GAME_OVER);
}

static void test_end_capture_all(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.houses[0] = 3;
	board.houses[5] = 1;
	board.houses[6] = 1;
	execute_play(&board, PLAYER_0, 5);

	CHECK(board.status == GAME_FINISHED);
	CHECK(board.captured[0] == 5);
	CHECK(board.winner == WINNER_PLAYER_0);
}

static void test_win_by_score(void){
	Board board;
	empty_board(&board, PLAYER_0);
	board.captured[0] = 23;
	board.houses[5] = 1;
	board.houses[6] = 1;
	board.houses[7] = 5;
	execute_play(&board, PLAYER_0, 5);

	CHECK(board.status == GAME_FINISHED);
	CHECK(board.winner == WINNER_PLAYER_0);
	CHECK(board.captured[0] == 25);
	CHECK(board.houses[7] == 5);
}

static void test_draw(void){
	Board board;
	empty_board(&board, PLAYER_1);
	board.captured[0] = 21;
	board.captured[1] = 24;
	board.houses[11] = 1;
	board.houses[1] = 1;
	board.houses[2] = 1;
	execute_play(&board, PLAYER_1, 5);

	CHECK(board.status == GAME_FINISHED);
	CHECK(board.captured[0] == 24 && board.captured[1] == 24);
	CHECK(board.winner == WINNER_DRAW);
}

static void test_idle_moves(void){
	Board board;

	empty_board(&board, PLAYER_0);
	board.houses[0] = 1;
	board.houses[7] = 1;
	board.idle_moves = 50;
	execute_play(&board, PLAYER_0, 0);
	CHECK(board.idle_moves == 51);

	empty_board(&board, PLAYER_0);
	board.houses[5] = 1;
	board.houses[6] = 1;
	board.houses[7] = 5;
	board.idle_moves = 50;
	execute_play(&board, PLAYER_0, 5);
	CHECK(board.idle_moves == 0);

	empty_board(&board, PLAYER_0);
	board.captured[0] = 23;
	board.captured[1] = 21;
	board.houses[0] = 1;
	board.houses[6] = 1;
	board.idle_moves = MAX_IDLE_MOVES-1;
	execute_play(&board, PLAYER_0, 0);
	CHECK(board.status == GAME_FINISHED);
	CHECK(board.captured[0] == 24 && board.captured[1] == 22);
	CHECK(board.winner == WINNER_PLAYER_0);
}

static void test_random_games(void){
	unsigned seed = 12345;
	int bad = 0;

	for (int game = 0; game < 5000; game++){
		Board board;
		int out[HOUSES_PER_SIDE];
		init_board(&board, (Side)(game%2));

		for (int moves = 0; moves < 5000 && board.status == GAME_ONGOING; moves++){
			int count = get_valid_plays(&board, board.turn, out);
			if (count == 0){bad++; break;}
			seed = seed*1103515245u + 12345u;
			if (execute_play(&board, board.turn, out[(seed>>16)%count]) != PLAY_OK){bad++; break;}
			if (total_seeds(&board) != 48){bad++; break;}}

		if (board.status != GAME_FINISHED) bad++;}

	CHECK(bad == 0);
}

static void test_forfeit(void){
	Board board;
	Game game;
	init_board(&board, PLAYER_0);

	CHECK(forfeit(&board, PLAYER_0) == 0);
	CHECK(board.status == GAME_FINISHED);
	CHECK(board.winner == WINNER_PLAYER_1);
	CHECK(board.captured[0] == 0 && board.captured[1] == 0);
	CHECK(total_seeds(&board) == 48);
	CHECK(execute_play(&board, PLAYER_0, 0) == PLAY_GAME_OVER);

	CHECK(forfeit(&board, PLAYER_1) == -1);
	CHECK(board.winner == WINNER_PLAYER_1);

	init_board(&board, PLAYER_0);
	CHECK(forfeit(&board, PLAYER_1) == 0);
	CHECK(board.winner == WINNER_PLAYER_0);

	init_board(&board, PLAYER_0);
	CHECK(forfeit(&board, (Side)5) == -1);
	CHECK(board.status == GAME_ONGOING && board.winner == WINNER_NONE);

	empty_board(&board, PLAYER_0);
	board.captured[0] = 23;
	board.houses[5] = 1;
	board.houses[6] = 1;
	board.houses[7] = 5;
	execute_play(&board, PLAYER_0, 5);
	CHECK(board.winner == WINNER_PLAYER_0);
	CHECK(forfeit(&board, PLAYER_0) == -1);
	CHECK(board.winner == WINNER_PLAYER_0);

	init_game(&game, PLAYER_0);
	play_move(&game, PLAYER_0, 2);
	CHECK(forfeit(&game.board, PLAYER_1) == 0);
	CHECK(game.num_moves == 1);
	CHECK(play_move(&game, PLAYER_0, 1) == PLAY_GAME_OVER);
	CHECK(game.num_moves == 1);
}

static int load_from_text(Game *game, const char *content){
	FILE *file = fopen(TMP_FILE, "w");
	int result;
	if (file == NULL) return -2;
	fputs(content, file);
	fclose(file);
	result = load_game(game, TMP_FILE);
	remove(TMP_FILE);
	return result;
}

static void play_random_game(Game *game, Side first, unsigned *seed){
	int out[HOUSES_PER_SIDE];
	init_game(game, first);

	while (game->board.status == GAME_ONGOING){
		int count = get_valid_plays(&game->board, game->board.turn, out);
		if (count == 0) break;
		*seed = (*seed)*1103515245u + 12345u;
		play_move(game, game->board.turn, out[((*seed)>>16)%count]);}
}

static void test_history_record(void){
	Game game;
	init_game(&game, PLAYER_1);

	CHECK(game.num_moves == 0);
	CHECK(game.first_player == PLAYER_1);
	CHECK(game.board.turn == PLAYER_1);

	CHECK(play_move(&game, PLAYER_1, 2) == PLAY_OK);
	CHECK(play_move(&game, PLAYER_1, 3) == PLAY_NOT_YOUR_TURN);
	CHECK(play_move(&game, PLAYER_0, 9) == PLAY_INVALID_HOUSE);
	CHECK(game.num_moves == 1 && game.moves[0] == 2);

	CHECK(play_move(&game, PLAYER_0, 0) == PLAY_OK);
	CHECK(game.num_moves == 2 && game.moves[1] == 0);
}

static void test_history_replay(void){
	Game game;
	Board expected, replayed;
	unsigned seed = 777;
	play_random_game(&game, PLAYER_0, &seed);

	init_board(&expected, game.first_player);
	for (int step = 0; step <= game.num_moves; step++){
		CHECK(replay_game(&game, step, &replayed) == 0);
		CHECK(memcmp(&replayed, &expected, sizeof(Board)) == 0);
		if (step < game.num_moves) execute_play(&expected, expected.turn, game.moves[step]);}

	CHECK(replay_game(&game, game.num_moves+50, &replayed) == 0);
	CHECK(memcmp(&replayed, &game.board, sizeof(Board)) == 0);
}

static void test_history_save_load(void){
	unsigned seed = 4242;

	for (int n = 0; n < 200; n++){
		Game original, loaded;
		play_random_game(&original, (Side)(n%2), &seed);

		CHECK(save_game(&original, TMP_FILE) == 0);
		CHECK(load_game(&loaded, TMP_FILE) == 0);
		CHECK(loaded.first_player == original.first_player);
		CHECK(loaded.num_moves == original.num_moves);
		CHECK(memcmp(loaded.moves, original.moves, (size_t)original.num_moves) == 0);
		CHECK(memcmp(&loaded.board, &original.board, sizeof(Board)) == 0);}

	remove(TMP_FILE);
}

static void test_history_load_invalid(void){
	Game game;

	CHECK(load_game(&game, "file_that_does_not_exist.tmp") == -1);
	CHECK(load_from_text(&game, "") == -1);
	CHECK(load_from_text(&game, "7 1 2\n") == -1);
	CHECK(load_from_text(&game, "0 9\n") == -1);
	CHECK(load_from_text(&game, "0 -1\n") == -1);
	CHECK(load_from_text(&game, "0 0 1 0\n") == -1);
	CHECK(load_from_text(&game, "1 2 5 abc\n") == -1);
	CHECK(load_from_text(&game, "0 2 x 3\n") == -1);

	CHECK(load_from_text(&game, "1\n") == 0);
	CHECK(game.num_moves == 0 && game.first_player == PLAYER_1);
	CHECK(load_from_text(&game, "1 2 5 0\n") == 0);
	CHECK(game.num_moves == 3 && game.moves[0] == 2 && game.moves[1] == 5 && game.moves[2] == 0);
	CHECK(save_game(&game, "no_such_dir/x.tmp") == -1);
}

static void test_history_limit(void){
	unsigned seed = 2026;
	int longest = 0;

	for (int n = 0; n < 5000; n++){
		Game game;
		play_random_game(&game, (Side)(n%2), &seed);
		if (game.num_moves > longest) longest = game.num_moves;}

	CHECK(longest < MAX_MOVES);
}

int main(void){
	test_init();
	test_validation();
	test_turn_change();
	test_sowing();
	test_simple_capture();
	test_chain_capture();
	test_chain_stops();
	test_no_capture_own_side();
	test_player1_capture();
	test_feed_rule();
	test_valid_plays();
	test_end_cannot_feed();
	test_end_capture_all();
	test_win_by_score();
	test_draw();
	test_idle_moves();
	test_random_games();
	test_forfeit();
	test_history_record();
	test_history_replay();
	test_history_save_load();
	test_history_load_invalid();
	test_history_limit();

	printf("%d passed, %d failed\n", passed, failed);
	return (failed == 0) ? 0 : 1;
}
