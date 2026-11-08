#ifndef USERS_H
#define USERS_H

#include "protocol.h"

/*
 * Persistent profile of a user, kept across connections in data/users.txt.
 * Friendships are in the friend graph (friends.h), keyed by `id`.
 */
typedef struct {
  int id; /* position in the user table, never changes */
  char name[NAME_LEN + 1];
  char bio[BIO_LEN + 1];
  int wins, losses, draws;
} User;

void users_load(const char *path);
int users_save(void);
/* profile of `name`, created if `create` is set; NULL if unknown or the table
 * is full */
User *users_get(const char *name, int create);
User *users_by_id(int id);
int users_count(void);
/* fills out with the users sorted by score (3 per win, 1 per draw), returns how
 * many */
int users_ranking(User **out, int max);

#endif
