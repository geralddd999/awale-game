#include "game.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>

/*
TODO:
	Capture logic
	Let the opponent play
	Winning and stop conditions
*/


static int abs_index(Side side, int house){
	return (side==PLAYER_0) ? house : house+HOUSES_PER_SIDE;
}

void init_board(Board *board, Side first_player){
	for (int i = 0; i<NUM_HOUSES; i++) board->houses[i] = INITIAL_SEEDS;
	board->captured[0] = board->captured[1] = 0;
	board->turn = first_player;
	board->status = GAME_ONGOING;
	board->winner = WINNER_NONE;}

PlayResult check_play(const Board *board, Side side, int house_id){
	if (board->status == GAME_FINISHED) return PLAY_GAME_OVER;
	if (board->turn != side) return PLAY_NOT_YOUR_TURN;
	if (house_id < 0 || house_id >= HOUSES_PER_SIDE) return PLAY_INVALID_HOUSE;
	if (board->houses[abs_index(side, house_id)] <= 0) return PLAY_EMPTY_HOUSE;
	return PLAY_OK;
}

int get_valid_plays(const Board *board, Side side, int out[HOUSES_PER_SIDE]){
	

}	

PlayResult execute_play(Board *board, Side side, int house_id){
	PlayResult check = check_play(board, side, house_id);
	if (check != PLAY_OK) return check;

	int og_index = abs_index(side, house_id);
	int seeds = board->houses[og_index];
	int curr_index = og_index;
	board->houses[og_index] = 0;

	while (seeds > 0){
		curr_index = (curr_index+1)%NUM_HOUSES;
		if (curr_index == og_index) continue;
		board->houses[curr_index]++;
		seeds--;}

	return check;
}
			

void print_board(const Board *board){
	for (int i = HOUSES_PER_SIDE-1; i>=0; i--)
		printf("%d ", board->houses[i]);
	
	printf("\n");

	for (int i=HOUSES_PER_SIDE; i<NUM_HOUSES; i++)
		printf("%d ", board->houses[i]);
	
	printf("\n");
}

int main(){
	srand(time(NULL));
	int value = rand();

	Board *board = malloc(sizeof(Board));	
	if (board == NULL) return 1;

	Side first_player;
	if (value%2==0) first_player = PLAYER_0;
	else first_player = PLAYER_1;

	init_board(board, first_player);
	
	
	print_board(board);

	printf("\n");
	execute_play(board, PLAYER_1, 2);


	print_board(board);

	free(board);
	return 0;
}
