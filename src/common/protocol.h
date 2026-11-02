#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Limits shared by client and server. The message format itself is described in PROTOCOL.md.
 */

#define NAME_LEN          16   /* usernames: [A-Za-z0-9_]{1,16} */
#define BIO_LEN           200
#define MAX_CLIENTS       100
#define MAX_LOBBIES       MAX_CLIENTS
#define MAX_LOBBY_MEMBERS 16   /* the two players + spectators */
#define MAX_INVITES       16
#define MAX_FRIENDS       32
#define MAX_USERS         1024

#endif
