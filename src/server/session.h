#ifndef SESSION_H
#define SESSION_H

#include <time.h>
#include "net.h"
#include "protocol.h"
#include "users.h"

typedef enum {
	ST_FREE,      /* slot unused */
	ST_UNAUTH,    /* connected, waiting for /login */
	ST_IDLE,      /* logged in, in the hall */
	ST_IN_LOBBY   /* logged in, member of a lobby (player or spectator) */
} ClientState;

typedef struct {
	SOCKET sock;
	ClientState state;
	/* set when the connection must be closed; the main loop reaps it after the current command */
	int dead;
	LineBuf in;
	char name[NAME_LEN+1];
	int lobby;    /* index in the lobby table, -1 when not in a lobby */
	User *user;   /* persistent profile, NULL before /login */
	/* at most one pending invitation: the inviting lobby (-1 if none) and when it expires */
	int invite_lobby;
	time_t invite_expires;
} Client;

/*
 * Clients live in fixed slots that never move, so lobbies can refer to them by index.
 */
extern Client clients[MAX_CLIENTS];

void session_init(void);
int session_alloc(SOCKET sock);
void session_free(int slot);
/* slot of the logged-in client with that name, -1 if not online */
int session_find(const char *name);
int session_logged(int slot);

/* a failed send marks the client dead instead of killing the server */
void session_send(int slot, const char *fmt, ...);
/* every logged-in client in the hall (not in a lobby) except `except` */
void session_broadcast_hall(int except, const char *fmt, ...);

#endif
