#ifndef LOBBY_H
#define LOBBY_H

#include <stddef.h>
#include <time.h>
#include "game.h"
#include "protocol.h"

typedef enum {
	LOBBY_FREE,
	LOBBY_WAITING,   /* open, no game running */
	LOBBY_PLAYING
} LobbyStatus;

/*
 * A lobby is named after its leader. The leader always holds seat 0 (PLAYER_0 in the game),
 * seat 1 is the opponent, every other member is a spectator. All values are client slots.
 */
typedef struct {
	LobbyStatus status;
	int leader;
	int seat[2];
	int members[MAX_LOBBY_MEMBERS];
	int nmembers;
	int is_private;
	Game game;
} Lobby;

/*
 * The functions returning a const char * return NULL on success, or "CODE text" for an ERR reply.
 * They send the events to the members themselves.
 */
void lobby_init(void);
int lobby_find(const char *leader);
const char *lobby_name(int id);
void lobby_list(int slot);

const char *lobby_create(int slot, int is_private);
const char *lobby_join(int id, int slot);
const char *lobby_invite(int id, int by, int target);
/* answer the pending invitation; `leader` may be empty or must name the inviting lobby */
const char *lobby_accept(int slot, const char *leader);
const char *lobby_deny(int slot, const char *leader);
const char *lobby_kick(int id, int by, const char *target);
const char *lobby_start(int id, int by);
const char *lobby_move(int id, int slot, int house);
const char *lobby_forfeit(int id, int slot);
/* reason: "left" or "disconnect" */
void lobby_leave(int slot, const char *reason);

/* invitations expire after INVITE_TIMEOUT seconds (changeable for tests) */
void lobby_set_invite_timeout(int seconds);
void lobby_expire_invites(time_t now);
/* seconds until the next invitation expires, -1 when none is pending */
int lobby_next_expiry(time_t now);
void lobby_broadcast(int id, int except, const char *fmt, ...);
/* "BOARD <name> <turn> <winner> h0 ... h11 cap0 cap1" */
void board_line(char *buf, size_t size, const char *name, const Board *b);

#endif
