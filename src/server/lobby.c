#include "lobby.h"
#include "session.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Lobby lobbies[MAX_LOBBIES];

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

	session_send(to, "EVT START %s %s %s %s", lobby_name(id),
	             clients[l->seat[0]].name, clients[l->seat[1]].name, role);
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

static int find_invite(const Lobby *l, const char *name){
	for (int i = 0; i<l->ninvited; i++){
		if (strcmp(l->invited[i], name) == 0) return i;
	}
	return -1;
}

static void remove_invite(Lobby *l, const char *name){
	int i = find_invite(l, name);
	if (i < 0) return;
	memmove(l->invited[i], l->invited[i+1], (l->ninvited-i-1) * sizeof l->invited[0]);
	l->ninvited--;
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
	l->next_first = opponent(l->game.first_player);
}

static void close_lobby(int id, const char *reason){
	Lobby *l = &lobbies[id];
	char name[NAME_LEN+1];
	snprintf(name, sizeof name, "%s", lobby_name(id));

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
	l->next_first = (Side)(rand()%2);

	clients[slot].state = ST_IN_LOBBY;
	clients[slot].lobby = id;

	const char *visibility = is_private ? "private" : "public";
	session_send(slot, "OK create %s %s", lobby_name(id), visibility);
	session_broadcast_hall(-1, "EVT LOBBY_OPEN %s %s", lobby_name(id), visibility);
	return NULL;
}

const char *lobby_join(int id, int slot){
	Lobby *l = &lobbies[id];
	const char *name = clients[slot].name;

	if (l->is_private && find_invite(l, name) < 0 && !users_is_friend(clients[l->leader].user, name))
		return "PRIVATE_LOBBY this lobby is private, ask the leader for an invite";
	if (l->nmembers == MAX_LOBBY_MEMBERS) return "LOBBY_FULL this lobby is full";

	remove_invite(l, name);
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

const char *lobby_invite(int id, int by, int target){
	Lobby *l = &lobbies[id];
	const char *name = clients[target].name;

	if (by != l->leader) return "NOT_LEADER only the leader can invite";
	if (clients[target].lobby == id) return "BAD_ARGS already in this lobby";

	if (find_invite(l, name) < 0){
		/* forget the oldest invitation when the list is full */
		if (l->ninvited == MAX_INVITES) remove_invite(l, l->invited[0]);
		snprintf(l->invited[l->ninvited++], NAME_LEN+1, "%s", name);
	}

	session_send(target, "EVT INVITE %s", lobby_name(id));
	session_send(by, "OK invite %s", name);
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
	remove_invite(l, name);
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

	init_game(&l->game, l->next_first);
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
