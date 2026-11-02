#include "commands.h"
#include "session.h"
#include "lobby.h"
#include "history.h"
#include "friends.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define S_UNAUTH (1u << ST_UNAUTH)
#define S_IDLE   (1u << ST_IDLE)
#define S_LOBBY  (1u << ST_IN_LOBBY)
#define S_LOGGED (S_IDLE | S_LOBBY)
#define S_ANY    (S_UNAUTH | S_LOGGED)

#define HISTORY_MAX 20
#define RANK_MAX    10

typedef void (*Handler)(int slot, char *args);

typedef struct {
	const char *name;
	unsigned states;   /* states in which the command is allowed */
	Handler run;
} Command;

static void err(int slot, const char *code_and_text){
	session_send(slot, "ERR %s", code_and_text);
}

/* cuts the next space-separated word out of *args */
static char *next_word(char **args){
	char *s = *args;
	while (*s == ' ') s++;
	char *word = s;
	while (*s != '\0' && *s != ' ') s++;
	if (*s != '\0') *s++ = '\0';
	*args = s;
	return word;
}

/* the rest of the line, without leading spaces: used for free text */
static char *rest(char *args){
	while (*args == ' ') args++;
	return args;
}

static int valid_name(const char *name){
	size_t len = strlen(name);
	if (len == 0 || len > NAME_LEN) return 0;
	for (size_t i = 0; i<len; i++){
		if (!isalnum((unsigned char)name[i]) && name[i] != '_') return 0;
	}
	return 1;
}

static int parse_int(const char *s, int *out){
	char *end;
	if (*s == '\0') return 0;
	long v = strtol(s, &end, 10);
	if (*end != '\0') return 0;
	*out = (int)v;
	return 1;
}

static void cmd_login(int slot, char *args){
	char *name = next_word(&args);
	Client *c = &clients[slot];

	if (!valid_name(name) || *rest(args) != '\0') { err(slot, "BAD_NAME names are 1-16 letters, digits or _"); return; }
	if (session_find(name) >= 0) { err(slot, "NAME_TAKEN this user is already connected"); return; }

	c->user = users_get(name, 1);
	if (c->user == NULL) { err(slot, "SERVER_FULL too many users"); return; }
	users_save();

	snprintf(c->name, sizeof c->name, "%s", name);
	c->state = ST_IDLE;
	session_send(slot, "OK login %s", name);
	session_broadcast_hall(slot, "EVT USER_JOIN %s", name);
}

static void cmd_list(int slot, char *args){
	(void)args;
	int n = 0;
	for (int i = 0; i<MAX_CLIENTS; i++) n += session_logged(i);

	session_send(slot, "OK list %d", n);
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (!session_logged(i)) continue;
		if (clients[i].state == ST_IN_LOBBY) session_send(slot, "USER %s lobby %s", clients[i].name, lobby_name(clients[i].lobby));
		else session_send(slot, "USER %s idle", clients[i].name);
	}
}

static void cmd_lobbies(int slot, char *args){
	(void)args;
	lobby_list(slot);
}

static void cmd_create(int slot, char *args){
	char *mode = next_word(&args);
	int is_private = strcmp(mode, "private") == 0;
	if (!is_private && *mode != '\0') { err(slot, "BAD_ARGS usage: /create [private]"); return; }

	const char *e = lobby_create(slot, is_private);
	if (e) err(slot, e);
}

static void cmd_join(int slot, char *args){
	char *leader = next_word(&args);
	if (*leader == '\0') { err(slot, "BAD_ARGS usage: /join <leader>"); return; }

	int id = lobby_find(leader);
	if (id < 0) { err(slot, "NO_LOBBY no lobby with this name"); return; }

	const char *e = lobby_join(id, slot);
	if (e) err(slot, e);
}

static void cmd_leave(int slot, char *args){
	(void)args;
	session_send(slot, "OK leave");
	lobby_leave(slot, "left");
}

static void cmd_invite(int slot, char *args){
	char *name = next_word(&args);
	int target = session_find(name);
	if (target < 0) { err(slot, "UNKNOWN_USER this user is not online"); return; }
	if (target == slot) { err(slot, "BAD_ARGS you cannot invite yourself"); return; }

	const char *e = lobby_invite(clients[slot].lobby, slot, target);
	if (e) err(slot, e);
}

static void cmd_accept(int slot, char *args){
	const char *e = lobby_accept(slot, next_word(&args));
	if (e) err(slot, e);
}

static void cmd_deny(int slot, char *args){
	const char *e = lobby_deny(slot, next_word(&args));
	if (e) err(slot, e);
}

static void cmd_kick(int slot, char *args){
	const char *e = lobby_kick(clients[slot].lobby, slot, next_word(&args));
	if (e) err(slot, e);
}

static void cmd_start(int slot, char *args){
	(void)args;
	const char *e = lobby_start(clients[slot].lobby, slot);
	if (e) err(slot, e);
}

static void cmd_move(int slot, char *args){
	int house;
	if (!parse_int(next_word(&args), &house)) { err(slot, "BAD_ARGS usage: /move <1-6>"); return; }

	const char *e = lobby_move(clients[slot].lobby, slot, house);
	if (e) err(slot, e);
}

static void cmd_forfeit(int slot, char *args){
	(void)args;
	const char *e = lobby_forfeit(clients[slot].lobby, slot);
	if (e) err(slot, e);
}

/* chat goes to the room the sender is in: its lobby, or the hall */
static void cmd_say(int slot, char *args){
	char *text = rest(args);
	if (*text == '\0') { err(slot, "BAD_ARGS usage: /say <text>"); return; }

	if (clients[slot].state == ST_IN_LOBBY)
		lobby_broadcast(clients[slot].lobby, slot, "EVT CHAT %s %s", clients[slot].name, text);
	else
		session_broadcast_hall(slot, "EVT CHAT %s %s", clients[slot].name, text);
	session_send(slot, "OK say");
}

static void cmd_msg(int slot, char *args){
	char *name = next_word(&args);
	char *text = rest(args);
	int target = session_find(name);

	if (*text == '\0') { err(slot, "BAD_ARGS usage: /msg <name> <text>"); return; }
	if (target < 0) { err(slot, "UNKNOWN_USER this user is not online"); return; }

	session_send(target, "EVT PM %s %s", clients[slot].name, text);
	session_send(slot, "OK msg %s", name);
}

static void cmd_bio(int slot, char *args){
	char *word = next_word(&args);

	if (strcmp(word, "set") == 0){
		User *me = clients[slot].user;
		snprintf(me->bio, sizeof me->bio, "%s", rest(args));
		users_save();
		session_send(slot, "OK bio");
		return;
	}

	User *u = users_get(*word ? word : clients[slot].name, 0);
	if (u == NULL) { err(slot, "UNKNOWN_USER no such user"); return; }
	session_send(slot, "OK bio %s %s", u->name, u->bio);
}

/* /friend alone lists the out-edges of your vertex in the friend graph */
static void list_friends(int slot){
	static int ids[MAX_USERS];
	int n = friends_of(clients[slot].user->id, ids, MAX_USERS);

	session_send(slot, "OK friends %d", n);
	for (int i = 0; i<n; i++){
		const char *name = users_by_id(ids[i])->name;
		session_send(slot, "FRIEND %s %s", name, session_find(name) >= 0 ? "online" : "offline");
	}
}

static void cmd_friend(int slot, char *args){
	char *name = next_word(&args);
	User *me = clients[slot].user;
	if (*name == '\0') { list_friends(slot); return; }

	User *other = users_get(name, 0);
	if (other == NULL) { err(slot, "UNKNOWN_USER no such user"); return; }
	if (other == me) { err(slot, "BAD_ARGS you cannot befriend yourself"); return; }
	if (friends_add(me->id, other->id) < 0) { err(slot, "BAD_ARGS already your friend"); return; }
	friends_save();
	session_send(slot, "OK friend %s", name);
}

static void cmd_unfriend(int slot, char *args){
	char *name = next_word(&args);
	User *other = users_get(name, 0);
	if (other == NULL || friends_remove(clients[slot].user->id, other->id) < 0) { err(slot, "UNKNOWN_USER not in your friends"); return; }
	friends_save();
	session_send(slot, "OK unfriend %s", name);
}

static void cmd_history(int slot, char *args){
	GameRecord records[HISTORY_MAX];
	char *name = next_word(&args);
	int n = history_list(*name ? name : clients[slot].name, records, HISTORY_MAX);

	session_send(slot, "OK history %d", n);
	for (int i = 0; i<n; i++){
		GameRecord *r = &records[i];
		session_send(slot, "GAME %d %s %s %s %d %d %d", r->id, r->players[0], r->players[1],
		             r->winner, r->captured[0], r->captured[1], r->num_moves);
	}
}

/* /replay <id> [step]: the position of a saved game after `step` moves (default: the end) */
static void cmd_replay(int slot, char *args){
	static Game game;
	GameRecord r;
	Board board;
	char line[BUF_SIZE], name[16];
	int id, step;

	if (!parse_int(next_word(&args), &id)) { err(slot, "BAD_ARGS usage: /replay <id> [step]"); return; }
	if (history_find(id, &r) < 0 || history_load(id, &game) < 0) { err(slot, "NO_GAME no saved game with this id"); return; }

	char *s = next_word(&args);
	if (*s == '\0') step = game.num_moves;
	else if (!parse_int(s, &step)) { err(slot, "BAD_ARGS usage: /replay <id> [step]"); return; }
	if (step < 0) step = 0;
	if (step > game.num_moves) step = game.num_moves;

	replay_game(&game, step, &board);
	snprintf(name, sizeof name, "#%d", id);
	board_line(line, sizeof line, name, &board);

	session_send(slot, "OK replay %d %d %d %s %s", id, step, game.num_moves, r.players[0], r.players[1]);
	session_send(slot, "%s", line);
}

static void cmd_rank(int slot, char *args){
	(void)args;
	User *top[RANK_MAX];
	int n = users_ranking(top, RANK_MAX);

	session_send(slot, "OK rank %d", n);
	for (int i = 0; i<n; i++)
		session_send(slot, "RANK %d %s %d %d %d", i+1, top[i]->name, top[i]->wins, top[i]->losses, top[i]->draws);
}

static void cmd_quit(int slot, char *args){
	(void)args;
	session_send(slot, "OK quit");
	clients[slot].dead = 1;
}

static void cmd_help(int slot, char *args);

static const Command commands[] = {
	{ "/login",    S_UNAUTH, cmd_login },
	{ "/list",     S_LOGGED, cmd_list },
	{ "/lobbies",  S_LOGGED, cmd_lobbies },
	{ "/create",   S_IDLE,   cmd_create },
	{ "/join",     S_IDLE,   cmd_join },
	{ "/leave",    S_LOBBY,  cmd_leave },
	{ "/invite",   S_LOBBY,  cmd_invite },
	{ "/accept",   S_IDLE,   cmd_accept },
	{ "/deny",     S_IDLE,   cmd_deny },
	{ "/kick",     S_LOBBY,  cmd_kick },
	{ "/start",    S_LOBBY,  cmd_start },
	{ "/move",     S_LOBBY,  cmd_move },
	{ "/forfeit",  S_LOBBY,  cmd_forfeit },
	{ "/say",      S_LOGGED, cmd_say },
	{ "/msg",      S_LOGGED, cmd_msg },
	{ "/bio",      S_LOGGED, cmd_bio },
	{ "/friend",   S_LOGGED, cmd_friend },
	{ "/unfriend", S_LOGGED, cmd_unfriend },
	{ "/history",  S_LOGGED, cmd_history },
	{ "/replay",   S_LOGGED, cmd_replay },
	{ "/rank",     S_LOGGED, cmd_rank },
	{ "/help",     S_ANY,    cmd_help },
	{ "/quit",     S_ANY,    cmd_quit },
};

#define NUM_COMMANDS (sizeof commands / sizeof commands[0])

static void cmd_help(int slot, char *args){
	(void)args;
	char list[BUF_SIZE] = "";
	for (size_t i = 0; i<NUM_COMMANDS; i++){
		strcat(list, " ");
		strcat(list, commands[i].name);
	}
	session_send(slot, "OK help%s", list);
}

void handle_line(int slot, char *line){
	char *args = line;
	char *name = next_word(&args);
	if (*name == '\0') return;

	for (size_t i = 0; i<NUM_COMMANDS; i++){
		if (strcmp(name, commands[i].name) != 0) continue;

		unsigned state = 1u << clients[slot].state;
		if (commands[i].states & state) commands[i].run(slot, args);
		else if (state == S_UNAUTH) err(slot, "NOT_LOGGED send /login <name> first");
		else if (commands[i].states == S_LOBBY) err(slot, "NO_LOBBY you are not in a lobby");
		else if (commands[i].states == S_IDLE) err(slot, "ALREADY_IN_LOBBY /leave your lobby first");
		else err(slot, "BAD_STATE command not allowed now");
		return;
	}
	err(slot, "UNKNOWN_CMD unknown command, try /help");
}
