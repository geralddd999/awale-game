#include "lobby.h"
#include "session.h"
#include "history.h"
#include "friends.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Lobby lobbies[MAX_LOBBIES];
static int invite_timeout = INVITE_TIMEOUT;

void lobby_set_invite_timeout(int seconds){
	if (seconds > 0) invite_timeout = seconds;
}

void lobby_init(void){
	for (int i = 0; i<MAX_LOBBIES; i++) lobbies[i].status = LOBBY_FREE;
}

const char *lobby_name(int id){
	return clients[lobbies[id].leader].name;
}

int lobby_find(const char *leader){
	int slot = session_find(leader);
	if (slot < 0 || clients[slot].state != ST_IN_LOBBY) return -1;

	int id = clients[slot].lobby;
	return (lobbies[id].leader == slot) ? id : -1;
}

void lobby_broadcast(int id, int except, const char *fmt, ...){
	char msg[BUF_SIZE];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof msg, fmt, args);
	va_end(args);

	Lobby *l = &lobbies[id];
	for (int i = 0; i<l->nmembers; i++){
		if (l->members[i] != except) session_send(l->members[i], "%s", msg);
	}
}

/* PLAYER_0 / PLAYER_1 for a seated client, -1 for a spectator */
static int side_of(const Lobby *l, int slot){
	if (l->seat[0] == slot) return PLAYER_0;
	if (l->seat[1] == slot) return PLAYER_1;
	return -1;
}

void board_line(char *buf, size_t size, const char *name, const Board *b){
	int len = snprintf(buf, size, "BOARD %s %d %d", name, b->turn, b->winner);
	for (int i = 0; i<NUM_HOUSES; i++) len += snprintf(buf+len, size-len, " %d", b->houses[i]);
	snprintf(buf+len, size-len, " %d %d", b->captured[0], b->captured[1]);
}

/* to = -1 sends the board to every member */
static void send_board(int id, int to){
	char msg[BUF_SIZE];
	board_line(msg, sizeof msg, lobby_name(id), &lobbies[id].game.board);
	if (to >= 0) session_send(to, "%s", msg);
	else lobby_broadcast(id, -1, "%s", msg);
}

static void send_start(int id, int to){
	Lobby *l = &lobbies[id];
	int side = side_of(l, to);
	char role[16];
	if (side < 0) snprintf(role, sizeof role, "spectator");
	else snprintf(role, sizeof role, "%d", side);

	session_send(to, "EVT START %s %s %s %s %s", lobby_name(id),
	             clients[l->seat[0]].name, clients[l->seat[1]].name, role,
	             clients[l->seat[l->game.first_player]].name);
}

void lobby_list(int slot){
	int n = 0;
	for (int i = 0; i<MAX_LOBBIES; i++) n += lobbies[i].status != LOBBY_FREE;
	session_send(slot, "OK lobbies %d", n);

	for (int i = 0; i<MAX_LOBBIES; i++){
		Lobby *l = &lobbies[i];
		if (l->status == LOBBY_FREE) continue;
		int spectators = l->nmembers - 1 - (l->seat[1] >= 0);
		session_send(slot, "LOBBY %s %s %s %s %d", lobby_name(i),
		             l->status == LOBBY_PLAYING ? "playing" : "waiting",
		             l->is_private ? "private" : "public",
		             l->seat[1] >= 0 ? clients[l->seat[1]].name : "-", spectators);
	}
}

/*
 * Ends the pending invitation of `target`, if any. With an event, the leader is told
 * "EVT <event> <leader> <target>", and so is the target when notify_target is set.
 */
static void invite_end(int target, const char *event, int notify_target){
	int id = clients[target].invite_lobby;
	if (id < 0) return;
	clients[target].invite_lobby = -1;
	if (event == NULL) return;

	if (notify_target) session_send(target, "EVT %s %s %s", event, lobby_name(id), clients[target].name);
	session_send(lobbies[id].leader, "EVT %s %s %s", event, lobby_name(id), clients[target].name);
}

void lobby_expire_invites(time_t now){
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (clients[i].state != ST_FREE && clients[i].invite_lobby >= 0 && now >= clients[i].invite_expires)
			invite_end(i, "INVITE_EXPIRED", 1);
	}
}

int lobby_next_expiry(time_t now){
	int next = -1;
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (clients[i].state == ST_FREE || clients[i].invite_lobby < 0) continue;
		int left = clients[i].invite_expires > now ? (int)(clients[i].invite_expires - now) : 0;
		if (next < 0 || left < next) next = left;
	}
	return next;
}

/* removes a member from the lobby and sends it back to the hall */
static void detach(int id, int slot){
	Lobby *l = &lobbies[id];
	for (int i = 0; i<l->nmembers; i++){
		if (l->members[i] != slot) continue;
		memmove(&l->members[i], &l->members[i+1], (l->nmembers-i-1) * sizeof(int));
		l->nmembers--;
		break;
	}
	if (l->seat[1] == slot) l->seat[1] = -1;

	clients[slot].state = ST_IDLE;
	clients[slot].lobby = -1;
}

/* when seat 1 is free between games, the oldest spectator takes it */
static void fill_seat(int id){
	Lobby *l = &lobbies[id];
	if (l->seat[1] >= 0 || l->status != LOBBY_WAITING) return;

	for (int i = 0; i<l->nmembers; i++){
		if (l->members[i] == l->seat[0]) continue;
		l->seat[1] = l->members[i];
		lobby_broadcast(id, -1, "EVT SEAT %s %s", lobby_name(id), clients[l->seat[1]].name);
		return;
	}
}

static void end_game(int id, const char *reason){
	Lobby *l = &lobbies[id];
	Board *b = &l->game.board;
	Client *p0 = &clients[l->seat[0]];
	Client *p1 = &clients[l->seat[1]];
	const char *winner = "draw";

	if (b->winner == WINNER_PLAYER_0){
		winner = p0->name;
		p0->user->wins++;
		p1->user->losses++;
	}
	else if (b->winner == WINNER_PLAYER_1){
		winner = p1->name;
		p1->user->wins++;
		p0->user->losses++;
	}
	else {
		p0->user->draws++;
		p1->user->draws++;
	}
	users_save();

	int game_id = history_record(&l->game, p0->name, p1->name);
	lobby_broadcast(id, -1, "EVT END %s %s %d %d %s %d", lobby_name(id), winner,
	                b->captured[0], b->captured[1], reason, game_id);

	l->status = LOBBY_WAITING;
}

static void close_lobby(int id, const char *reason){
	Lobby *l = &lobbies[id];
	char name[NAME_LEN+1];
	snprintf(name, sizeof name, "%s", lobby_name(id));

	/* invitations to a closed lobby cannot be accepted any more */
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (clients[i].state != ST_FREE && clients[i].invite_lobby == id) invite_end(i, "INVITE_CANCELLED", 1);
	}

	/* tell the hall first, the members are not in it yet */
	session_broadcast_hall(-1, "EVT LOBBY_CLOSED %s %s", name, reason);
	for (int i = 0; i<l->nmembers; i++){
		int m = l->members[i];
		if (m != l->leader) session_send(m, "EVT LOBBY_CLOSED %s %s", name, reason);
		clients[m].state = ST_IDLE;
		clients[m].lobby = -1;
	}
	l->status = LOBBY_FREE;
}

const char *lobby_create(int slot, int is_private){
	int id = -1;
	for (int i = 0; i<MAX_LOBBIES && id < 0; i++){
		if (lobbies[i].status == LOBBY_FREE) id = i;
	}
	if (id < 0) return "SERVER_FULL no lobby available";

	Lobby *l = &lobbies[id];
	memset(l, 0, sizeof(Lobby));
	l->status = LOBBY_WAITING;
	l->leader = slot;
	l->seat[0] = slot;
	l->seat[1] = -1;
	l->members[0] = slot;
	l->nmembers = 1;
	l->is_private = is_private;

	invite_end(slot, "INVITE_CANCELLED", 1);
	clients[slot].state = ST_IN_LOBBY;
	clients[slot].lobby = id;

	const char *visibility = is_private ? "private" : "public";
	session_send(slot, "OK create %s %s", lobby_name(id), visibility);
	session_broadcast_hall(-1, "EVT LOBBY_OPEN %s %s", lobby_name(id), visibility);
	return NULL;
}

/* `invited` lets the client into a private lobby */
static const char *enter(int id, int slot, int invited){
	Lobby *l = &lobbies[id];
	const char *name = clients[slot].name;

	if (!invited && l->is_private && !friends_has(clients[l->leader].user->id, clients[slot].user->id))
		return "PRIVATE_LOBBY this lobby is private, ask the leader for an invite";
	if (l->nmembers == MAX_LOBBY_MEMBERS) return "LOBBY_FULL this lobby is full";

	/* entering the inviting lobby answers the invitation, entering another one cancels it */
	invite_end(slot, clients[slot].invite_lobby == id ? NULL : "INVITE_CANCELLED", 1);
	l->members[l->nmembers++] = slot;
	clients[slot].state = ST_IN_LOBBY;
	clients[slot].lobby = id;

	const char *role = "spectator";
	if (l->seat[1] < 0 && l->status == LOBBY_WAITING){
		l->seat[1] = slot;
		role = "player";
	}

	session_send(slot, "OK join %s %s", lobby_name(id), role);
	lobby_broadcast(id, slot, "EVT JOINED %s %s %s", lobby_name(id), name, role);

	/* a spectator arriving mid-game gets the current position */
	if (l->status == LOBBY_PLAYING){
		send_start(id, slot);
		send_board(id, slot);
	}
	return NULL;
}

const char *lobby_join(int id, int slot){
	return enter(id, slot, 0);
}

const char *lobby_invite(int id, int by, int target){
	Lobby *l = &lobbies[id];
	Client *c = &clients[target];

	if (by != l->leader) return "NOT_LEADER only the leader can invite";
	if (c->state == ST_IN_LOBBY) return "BUSY this user is already in a lobby";
	/* one invitation at a time: the user must answer it, or let it expire, before getting another */
	if (c->invite_lobby >= 0) return "BUSY this user already has a pending invitation";

	c->invite_lobby = id;
	c->invite_expires = time(NULL) + invite_timeout;
	session_send(target, "EVT INVITE %s %d", lobby_name(id), invite_timeout);
	session_send(by, "OK invite %s", c->name);
	return NULL;
}

/* the pending invitation of `slot`, checked against the leader name the user typed (may be empty) */
static const char *pending_invite(int slot, const char *leader, int *id){
	*id = clients[slot].invite_lobby;
	if (*id < 0) return "NO_INVITE you have no pending invitation";
	if (*leader != '\0' && strcmp(leader, lobby_name(*id)) != 0) return "NO_INVITE you have no invitation from this lobby";
	return NULL;
}

const char *lobby_accept(int slot, const char *leader){
	int id;
	const char *e = pending_invite(slot, leader, &id);
	return e ? e : enter(id, slot, 1);
}

const char *lobby_deny(int slot, const char *leader){
	int id;
	const char *e = pending_invite(slot, leader, &id);
	if (e) return e;

	session_send(slot, "OK deny %s", lobby_name(id));
	invite_end(slot, "INVITE_DENIED", 0);
	return NULL;
}

const char *lobby_kick(int id, int by, const char *name){
	Lobby *l = &lobbies[id];
	int target = session_find(name);

	if (by != l->leader) return "NOT_LEADER only the leader can kick";
	if (target < 0 || clients[target].lobby != id) return "UNKNOWN_USER this user is not in the lobby";
	if (target == by) return "BAD_ARGS use /leave to leave your own lobby";
	if (l->status == LOBBY_PLAYING && side_of(l, target) >= 0)
		return "BAD_STATE cannot kick a player during a game";

	session_send(target, "EVT KICKED %s", lobby_name(id));
	detach(id, target);
	session_send(by, "OK kick %s", name);
	lobby_broadcast(id, by, "EVT LEFT %s %s kicked", lobby_name(id), name);
	fill_seat(id);
	return NULL;
}

const char *lobby_start(int id, int by){
	Lobby *l = &lobbies[id];

	if (by != l->leader) return "NOT_LEADER only the leader can start the game";
	if (l->status == LOBBY_PLAYING) return "BAD_STATE a game is already running";
	if (l->seat[1] < 0) return "SEATS_EMPTY waiting for an opponent to join";

	/* the server draws who begins each game */
	init_game(&l->game, (Side)(rand()%2));
	l->status = LOBBY_PLAYING;

	session_send(by, "OK start");
	for (int i = 0; i<l->nmembers; i++) send_start(id, l->members[i]);
	send_board(id, -1);
	return NULL;
}

const char *lobby_move(int id, int slot, int house){
	Lobby *l = &lobbies[id];
	int side = side_of(l, slot);

	if (side < 0) return "NOT_PLAYER spectators cannot play";
	if (l->status != LOBBY_PLAYING) return "BAD_STATE no game is running";

	switch (play_move(&l->game, (Side)side, house-1)){
		case PLAY_OK: break;
		case PLAY_NOT_YOUR_TURN: return "NOT_YOUR_TURN wait for your opponent";
		case PLAY_INVALID_HOUSE: return "INVALID_HOUSE choose a house between 1 and 6";
		case PLAY_EMPTY_HOUSE: return "EMPTY_HOUSE this house is empty";
		case PLAY_MUST_FEED: return "MUST_FEED your opponent has no seeds, you must give them some";
		case PLAY_GAME_OVER: return "BAD_STATE the game is over";
	}

	session_send(slot, "OK move %d", house);
	lobby_broadcast(id, -1, "EVT MOVE %s %s %d", lobby_name(id), clients[slot].name, house);
	send_board(id, -1);
	if (l->game.board.status == GAME_FINISHED) end_game(id, "normal");
	return NULL;
}

const char *lobby_forfeit(int id, int slot){
	Lobby *l = &lobbies[id];
	int side = side_of(l, slot);

	if (side < 0) return "NOT_PLAYER spectators cannot forfeit";
	if (l->status != LOBBY_PLAYING) return "BAD_STATE no game is running";

	session_send(slot, "OK forfeit");
	forfeit(&l->game.board, (Side)side);
	end_game(id, "forfeit");
	return NULL;
}

void lobby_leave(int slot, const char *reason){
	/* a user who disconnects with a pending invitation never answers it */
	invite_end(slot, "INVITE_CANCELLED", 0);

	int id = clients[slot].lobby;
	if (id < 0) return;

	Lobby *l = &lobbies[id];
	int side = side_of(l, slot);

	/* a player walking out of a running game loses it */
	if (l->status == LOBBY_PLAYING && side >= 0){
		forfeit(&l->game.board, (Side)side);
		end_game(id, reason);
	}

	if (slot == l->leader){
		close_lobby(id, strcmp(reason, "left") == 0 ? "leader_left" : "leader_disconnected");
		return;
	}

	char name[NAME_LEN+1];
	snprintf(name, sizeof name, "%s", clients[slot].name);
	detach(id, slot);
	lobby_broadcast(id, -1, "EVT LEFT %s %s %s", lobby_name(id), name, reason);
	fill_seat(id);
}
