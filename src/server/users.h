#ifndef USERS_H
#define USERS_H

#include "protocol.h"

/*
 * Persistent profile of a user, kept across connections in data/users.txt.
 */
typedef struct {
	char name[NAME_LEN+1];
	char bio[BIO_LEN+1];
	char friends[MAX_FRIENDS][NAME_LEN+1];
	int nfriends;
	int wins, losses, draws;
} User;

void users_load(const char *path);
int users_save(void);
/* profile of `name`, created if `create` is set; NULL if unknown or the table is full */
User *users_get(const char *name, int create);
int users_is_friend(const User *u, const char *name);
int users_add_friend(User *u, const char *name);
int users_remove_friend(User *u, const char *name);
/* fills out with the users sorted by score (3 per win, 1 per draw), returns how many */
int users_ranking(User **out, int max);

#endif
