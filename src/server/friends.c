#include "friends.h"
#include "users.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Adjacency lists: out[a] is a linked list of the edges leaving vertex a.
 * Memory grows with the number of friendships instead of users^2.
 */
typedef struct Edge Edge;
struct Edge {
  int to;
  Edge *next;
};

static Edge *out[MAX_USERS];
static char friends_path[256];

void friends_load(const char *path) {
  char a[NAME_LEN + 1], b[NAME_LEN + 1];
  snprintf(friends_path, sizeof friends_path, "%s", path);

  FILE *file = fopen(path, "r");
  if (file == NULL)
    return;

  while (fscanf(file, "%16s %16s", a, b) == 2) {
    User *ua = users_get(a, 0);
    User *ub = users_get(b, 0);
    if (ua != NULL && ub != NULL)
      friends_add(ua->id, ub->id);
  }
  fclose(file);
}

int friends_save(void) {
  char tmp[300];
  snprintf(tmp, sizeof tmp, "%s.tmp", friends_path);

  FILE *file = fopen(tmp, "w");
  if (file == NULL)
    return -1;

  for (int a = 0; a < users_count(); a++) {
    for (Edge *e = out[a]; e != NULL; e = e->next)
      fprintf(file, "%s %s\n", users_by_id(a)->name, users_by_id(e->to)->name);
  }
  fclose(file);
  return rename(tmp, friends_path);
}

int friends_has(int from, int to) {
  for (Edge *e = out[from]; e != NULL; e = e->next) {
    if (e->to == to)
      return 1;
  }
  return 0;
}

int friends_add(int from, int to) {
  /* walk to the end of the list (checking for a duplicate on the way) and
   * append there, so friends are listed in the order they were added */
  Edge **link = &out[from];
  while (*link != NULL) {
    if ((*link)->to == to)
      return -1;
    link = &(*link)->next;
  }

  Edge *e = malloc(sizeof(Edge));
  if (e == NULL)
    return -1;
  e->to = to;
  e->next = NULL;
  *link = e;
  return 0;
}

int friends_remove(int from, int to) {
  for (Edge **link = &out[from]; *link != NULL; link = &(*link)->next) {
    if ((*link)->to != to)
      continue;

    Edge *e = *link;
    *link = e->next;
    free(e);
    return 0;
  }
  return -1;
}

int friends_of(int from, int *ids, int max) {
  int n = 0;
  for (Edge *e = out[from]; e != NULL && n < max; e = e->next)
    ids[n++] = e->to;
  return n;
}
