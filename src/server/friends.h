#ifndef FRIENDS_H
#define FRIENDS_H

/*
 * Friendships as a directed graph: users are the vertices (User.id), an edge a
 * -> b means "a added b as a friend". It is directed so that friending someone
 * does not give you access to their private lobbies; a mutual friendship is two
 * edges. Stored in data/friends.txt as one edge per line: "a b".
 */

void friends_load(const char *path);
int friends_save(void);

int friends_has(int from, int to);
/* return -1 if the edge already exists / does not exist */
int friends_add(int from, int to);
int friends_remove(int from, int to);
/* vertices reachable by one edge from `from`, returns how many */
int friends_of(int from, int *out, int max);

#endif
