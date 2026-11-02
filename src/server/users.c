#include "users.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static User users[MAX_USERS];
static int nusers = 0;
static char users_path[256];

/*
 * One user per line:
 *   name wins losses draws friend1,friend2,... bio
 */
void users_load(const char *path){
	char line[2048];
	snprintf(users_path, sizeof users_path, "%s", path);
	nusers = 0;

	FILE *file = fopen(path, "r");
	if (file == NULL) return;

	while (nusers < MAX_USERS && fgets(line, sizeof line, file) != NULL){
		User *u = &users[nusers];
		char friends[600];
		int bio_at = -1;

		line[strcspn(line, "\r\n")] = '\0';
		memset(u, 0, sizeof(User));
		if (sscanf(line, "%16s %d %d %d %599s %n", u->name, &u->wins, &u->losses, &u->draws, friends, &bio_at) < 5) continue;

		if (bio_at >= 0) snprintf(u->bio, sizeof u->bio, "%s", line+bio_at);
		if (strcmp(friends, "-") != 0){
			for (char *f = strtok(friends, ","); f != NULL; f = strtok(NULL, ","))
				users_add_friend(u, f);
		}
		nusers++;
	}
	fclose(file);
}

int users_save(void){
	char tmp[300];
	snprintf(tmp, sizeof tmp, "%s.tmp", users_path);

	/* write a temporary file then rename it, so a crash never leaves a half-written users file */
	FILE *file = fopen(tmp, "w");
	if (file == NULL) return -1;

	for (int i = 0; i<nusers; i++){
		User *u = &users[i];
		fprintf(file, "%s %d %d %d ", u->name, u->wins, u->losses, u->draws);
		if (u->nfriends == 0) fputc('-', file);
		for (int f = 0; f<u->nfriends; f++) fprintf(file, "%s%s", f ? "," : "", u->friends[f]);
		fprintf(file, " %s\n", u->bio);
	}
	fclose(file);
	return rename(tmp, users_path);
}

User *users_get(const char *name, int create){
	for (int i = 0; i<nusers; i++){
		if (strcmp(users[i].name, name) == 0) return &users[i];
	}
	if (!create || nusers == MAX_USERS) return NULL;

	User *u = &users[nusers++];
	memset(u, 0, sizeof(User));
	snprintf(u->name, sizeof u->name, "%s", name);
	return u;
}

int users_is_friend(const User *u, const char *name){
	for (int i = 0; i<u->nfriends; i++){
		if (strcmp(u->friends[i], name) == 0) return 1;
	}
	return 0;
}

int users_add_friend(User *u, const char *name){
	if (users_is_friend(u, name)) return 0;
	if (u->nfriends == MAX_FRIENDS) return -1;
	snprintf(u->friends[u->nfriends++], NAME_LEN+1, "%s", name);
	return 0;
}

int users_remove_friend(User *u, const char *name){
	for (int i = 0; i<u->nfriends; i++){
		if (strcmp(u->friends[i], name) != 0) continue;
		memmove(u->friends[i], u->friends[i+1], (u->nfriends-i-1) * sizeof u->friends[0]);
		u->nfriends--;
		return 0;
	}
	return -1;
}

static int score(const User *u){
	return 3*u->wins + u->draws;
}

static int by_score(const void *a, const void *b){
	const User *ua = *(User * const *)a;
	const User *ub = *(User * const *)b;
	if (score(ua) != score(ub)) return score(ub) - score(ua);
	return strcmp(ua->name, ub->name);
}

int users_ranking(User **out, int max){
	static User *all[MAX_USERS];
	for (int i = 0; i<nusers; i++) all[i] = &users[i];
	qsort(all, nusers, sizeof all[0], by_score);

	int n = nusers < max ? nusers : max;
	for (int i = 0; i<n; i++) out[i] = all[i];
	return n;
}
