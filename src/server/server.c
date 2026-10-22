#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <string.h>


#include "server.h"
#include "client.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

/* an array for all clients */
static Client clients[MAX_CLIENTS];
/* the index for the array and the count for connected users */
static int actual = 0;



/*
 * Returns -1 if a QUIT was requested by the client.
 * Returns >=0 for anyother command
 */
static int handle_command(Client *c, char* command){
  char *cmd_args = strchr(command, ' ');

  if(cmd_args != NULL){
    *cmd_args = '\0';
    cmd_args++;
  }else{
    cmd_args = "";
  }
  if(strstr(command, "/list") != NULL){
    //check for the other arguments;
    if(strstr(cmd_args, "online")){

      for(int i =0 ; i< actual; i++){
        if(strcmp(clients[i].username, c->username) != 0){
          // add a part that indicates if the user is currently in a game or spectating or free
          write_client(c->sock, clients[i].username);
        }
      }
    }
    if(strstr(command, "friends")){
      //algo for listing the friends;
    }
  }else if(strcmp(command, "/chat")){
      //send a msg to everyone, even in a game
  }else if(strcmp(command, "/challenge")){
    // check if player is in match before allowing to challenge
    // check if player exists
    // Don't allow player to challenge another player if invite sent;
  }else if(strcmp(command, "/accept")){
    //need to accept only a person, and then start a game
    // need to change the status of the two players to IN_GAME
    // can't accept any player it has to be the one that sent the invite

  }

  else if(strcmp(command, "/quit") == 0){

    return -1;
  }


  return 1;

}

/*
 * non blocking and full processing equivalent of the read_client function
 */
static int handle_client_input(Client *c){
  int n = recv(c->sock, c->inbuf + c->inlen, BUF_SIZE - c->inlen, 0);

  if(n <=0){
    return 0 ; //eof or error or disconnect
  }
  c->inlen += n;

  char *start = c->inbuf;
  char *end = c->inbuf + c->inlen;
  char *n1;

  //get every complete command in the buffer
  while((n1 = memchr(start, '\n', end-start)) != NULL){
    *n1 = '\0';
    if(n1 > start && n1[-1] =='\r'){
      n1[-1] = '\0';
    }

    //handle the command
      int cmd_code = handle_command(c, start);
      if(cmd_code == -1){
        // Client requested QUIT
        //remove client
        return 0;
      }
      start = n1+1;
  }

  int rest = end - start;
  memmove(c->inbuf, start, rest);
  c->inlen = rest;

  if (c->inlen == BUF_SIZE){
    return 0; //line too long exceeded
  }
  return 1;

}


static void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   //int actual = 0;
   int max = sock;
   //Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         socklen_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         // what happens if the client never sends its name
         if(read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = { csock };
         strncpy(c.username, buffer, BUF_SIZE - 1);
         clients[actual] = c;
         actual++;
      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               //int c = read_client(clients[i].sock, buffer);
               int code = handle_client_input(&clients[i]);
               /* client disconnected */
               if(code == 0 )
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.username, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}




static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != clients[i].sock)
      {
         if(from_server == 0)
         {
            strncpy(message, sender.username, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
