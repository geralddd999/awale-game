#include "users.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static User users[MAX_USERS];
static int nusers = 0;
static char users_path[256];

/*
 * One user per line:
 *   name wins losses draws bio text until the end of the line
 */
void users_load(const char *path){
	char line[2048];
	snprintf(users_path, sizeof users_path, "%s", path);
	nusers = 0;

	FILE *file = fopen(path, "r");
	if (file == NULL) return;

	while (nusers < MAX_USERS && fgets(line, sizeof line, file) != NULL){
		User *u = &users[nusers];
		int bio_at = -1;

		line[strcspn(line, "\r\n")] = '\0';
		memset(u, 0, sizeof(User));
		if (sscanf(line, "%16s %d %d %d %n", u->name, &u->wins, &u->losses, &u->draws, &bio_at) < 4) continue;

		if (bio_at >= 0) snprintf(u->bio, sizeof u->bio, "%s", line+bio_at);
		u->id = nusers++;
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
		fprintf(file, "%s %d %d %d %s\n", u->name, u->wins, u->losses, u->draws, u->bio);
	}
	fclose(file);
	return rename(tmp, users_path);
}

User *users_get(const char *name, int create){
	for (int i = 0; i<nusers; i++){
		if (strcmp(users[i].name, name) == 0) return &users[i];
	}
	if (!create || nusers == MAX_USERS) return NULL;

	User *u = &users[nusers];
	memset(u, 0, sizeof(User));
	u->id = nusers++;
	snprintf(u->name, sizeof u->name, "%s", name);
	return u;
}

User *users_by_id(int id){
	return (id >= 0 && id < nusers) ? &users[id] : NULL;
}

int users_count(void){
	return nusers;
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
