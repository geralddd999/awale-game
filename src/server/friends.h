#ifndef FRIENDS_H
#define FRIENDS_H

/*
 * Friendships as a directed graph: users are the vertices (User.id).
 * - A single edge a -> b without b -> a means "a sent a friend request to b".
 * - Both edges a -> b and b -> a means "a and b are mutual friends".
 * Stored in data/friends.txt as one edge per line: "a b".
 */

void friends_load(const char *path);
int friends_save(void);

int friends_has(int from, int to);
int friends_are_mutual(int a, int b);
/* return -1 if the edge already exists / does not exist */
int friends_add(int from, int to);
int friends_remove(int from, int to);
/* mutual friends of `user`, returns how many */
int friends_of(int user, int *out, int max);
/* users `from` who sent a pending request to `user` (from -> user, !user ->
 * from) */
int friends_requests_in(int user, int *out, int max);
/* users `to` to whom `user` sent a pending request (user -> to, !to -> user) */
int friends_requests_out(int user, int *out, int max);

#endif
