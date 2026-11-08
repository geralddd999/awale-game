#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Limits shared by client and server. The message format itself is described in
 * PROTOCOL.md.
 */

#define NAME_LEN 16 /* usernames: [A-Za-z0-9_]{1,16} */
#define BIO_LINES 10
#define BIO_LINE_LEN 80
#define BIO_LEN (BIO_LINES * (BIO_LINE_LEN + 2))
#define MAX_CLIENTS 100
#define MAX_LOBBIES MAX_CLIENTS
#define MAX_LOBBY_MEMBERS 16 /* the two players + spectators */
#define INVITE_TIMEOUT 30    /* seconds before a pending invitation expires */
#define MAX_USERS 1024

#endif
