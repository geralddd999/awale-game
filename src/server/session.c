#include "session.h"
#include <string.h>

Client clients[MAX_CLIENTS];

void session_init(void){
	for (int i = 0; i<MAX_CLIENTS; i++){
		clients[i].state = ST_FREE;
		clients[i].lobby = -1;
	}
}

int session_alloc(SOCKET sock){
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (clients[i].state != ST_FREE) continue;

		memset(&clients[i], 0, sizeof(Client));
		clients[i].sock = sock;
		clients[i].state = ST_UNAUTH;
		clients[i].lobby = -1;
		linebuf_init(&clients[i].in);
		return i;
	}
	return -1;
}

void session_free(int slot){
	closesocket(clients[slot].sock);
	clients[slot].state = ST_FREE;
	clients[slot].dead = 0;
	clients[slot].lobby = -1;
	clients[slot].user = NULL;
}

int session_logged(int slot){
	return clients[slot].state == ST_IDLE || clients[slot].state == ST_IN_LOBBY;
}

int session_find(const char *name){
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (session_logged(i) && strcmp(clients[i].name, name) == 0) return i;
	}
	return -1;
}

static void sendv(int slot, const char *fmt, va_list args){
	Client *c = &clients[slot];
	if (c->state == ST_FREE || c->dead) return;
	if (send_linev(c->sock, fmt, args) < 0) c->dead = 1;
}

void session_send(int slot, const char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	sendv(slot, fmt, args);
	va_end(args);
}

void session_broadcast_hall(int except, const char *fmt, ...){
	for (int i = 0; i<MAX_CLIENTS; i++){
		if (i == except || clients[i].state != ST_IDLE) continue;
		va_list args;
		va_start(args, fmt);
		sendv(i, fmt, args);
		va_end(args);
	}
}
