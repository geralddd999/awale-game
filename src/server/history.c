#include "history.h"
#include <stdio.h>
#include <string.h>

static char games_dir[256];
static int next_id = 1;

static int parse_record(const char *line, GameRecord *r) {
  return sscanf(line, "%d %16s %16s %16s %d %d %d", &r->id, r->players[0],
                r->players[1], r->winner, &r->captured[0], &r->captured[1],
                &r->num_moves) == 7;
}

static void index_path(char *buf, size_t size) {
  snprintf(buf, size, "%s/index.txt", games_dir);
}

void history_init(const char *dir) {
  char path[300], line[256];
  GameRecord r;
  snprintf(games_dir, sizeof games_dir, "%s", dir);

  index_path(path, sizeof path);
  FILE *file = fopen(path, "r");
  if (file == NULL)
    return;
  while (fgets(line, sizeof line, file) != NULL) {
    if (parse_record(line, &r) && r.id >= next_id)
      next_id = r.id + 1;
  }
  fclose(file);
}

int history_record(const Game *game, const char *p0, const char *p1) {
  char path[300];
  const Board *b = &game->board;
  const char *winner = "draw";
  if (b->winner == WINNER_PLAYER_0)
    winner = p0;
  if (b->winner == WINNER_PLAYER_1)
    winner = p1;

  int id = next_id;
  snprintf(path, sizeof path, "%s/%d.txt", games_dir, id);
  if (save_game(game, path) < 0)
    return -1;

  index_path(path, sizeof path);
  FILE *file = fopen(path, "a");
  if (file == NULL)
    return -1;
  fprintf(file, "%d %s %s %s %d %d %d\n", id, p0, p1, winner, b->captured[0],
          b->captured[1], game->num_moves);
  fclose(file);

  next_id++;
  return id;
}

int history_list(const char *name, GameRecord *out, int max) {
  char path[300], line[256];
  GameRecord r;
  int n = 0;

  if (max <= 0)
    return 0;
  index_path(path, sizeof path);
  FILE *file = fopen(path, "r");
  if (file == NULL)
    return 0;

  /* keep the last `max` matches in a ring, then return them newest first */
  static GameRecord ring[64];
  int total = 0;
  if (max > 64)
    max = 64;
  while (fgets(line, sizeof line, file) != NULL) {
    if (!parse_record(line, &r))
      continue;
    if (name != NULL && strcmp(r.players[0], name) != 0 &&
        strcmp(r.players[1], name) != 0)
      continue;
    ring[total % max] = r;
    total++;
  }
  fclose(file);

  for (int i = total - 1; i >= 0 && n < max; i--)
    out[n++] = ring[i % max];
  return n;
}

int history_find(int id, GameRecord *out) {
  char path[300], line[256];
  index_path(path, sizeof path);
  FILE *file = fopen(path, "r");
  if (file == NULL)
    return -1;

  int found = -1;
  while (found < 0 && fgets(line, sizeof line, file) != NULL) {
    if (parse_record(line, out) && out->id == id)
      found = 0;
  }
  fclose(file);
  return found;
}

int history_load(int id, Game *game) {
  char path[300];
  snprintf(path, sizeof path, "%s/%d.txt", games_dir, id);
  return load_game(game, path);
}
