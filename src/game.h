#ifndef GAME_H
#define GAME_H

#define NUM_HOUSES 12
#define HOUSES_PER_SIDE 6
#define INITIAL_SEEDS 4
#define WIN_SCORE 25
#define MAX_IDLE_MOVES 100

typedef enum {
	PLAYER_0 = 0,   
	PLAYER_1 = 1    
} Side;

typedef enum {
	GAME_ONGOING,
	GAME_FINISHED
} GameStatus;

typedef enum {
	WINNER_NONE,
	WINNER_PLAYER_0,
	WINNER_PLAYER_1,
	WINNER_DRAW
} Winner;

typedef enum {
	PLAY_OK,
	PLAY_GAME_OVER,
	PLAY_NOT_YOUR_TURN,
	PLAY_INVALID_HOUSE,   
	PLAY_EMPTY_HOUSE,
	PLAY_MUST_FEED        
} PlayResult;

typedef struct {
	int houses[NUM_HOUSES];
	int captured[2];   
	int idle_moves;
	Side turn;
	GameStatus status;
	Winner winner;
} Board;

void init_board(Board *board, Side first_player);

PlayResult execute_play(Board *board, Side side, int house);

PlayResult check_play(const Board *board, Side side, int house);

int get_valid_plays(const Board *board, Side side, int out[HOUSES_PER_SIDE]);

void print_board(const Board *board);

#endif
