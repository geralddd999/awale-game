#ifndef CLIENT_H
#define CLIENT_H

#include "config.h"

typedef struct
{
  /*
   * The socket that represents the connection between the client and the server
   */
   SOCKET sock;
   /*
    * The funny username of our user, used by the server to allocate matches / and as a login
    */
   char username[BUF_SIZE];
   /*
    * Represents the bytes recv'd but not yed processed infull.
    */
   char inbuf[BUF_SIZE];
   /*
    * Represents how many bytes are in the inbuf
    */
   int inlen;
}Client;


#endif /* guard */
