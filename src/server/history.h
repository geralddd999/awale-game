#ifndef HISTORY_H
#define HISTORY_H

#include "game.h"
#include "protocol.h"

/*
 * Finished games: data/games/<id>.txt ,
 * data/games/index.txt has one line per game: id p0 p1 winner|draw cap0 cap1
 * nmoves
 */
typedef struct {
  int id;
  char players[2][NAME_LEN + 1];
  char winner[NAME_LEN + 1];
  int captured[2];
  int num_moves;
} GameRecord;

void history_init(const char *dir);
/* saves the game and appends it to the index, returns its id or -1 */
int history_record(const Game *game, const char *p0, const char *p1);
/* last `max` records involving `name` (any player if NULL), most recent first
 */
int history_list(const char *name, GameRecord *out, int max);
int history_find(int id, GameRecord *out);
int history_load(int id, Game *game);

#endif
