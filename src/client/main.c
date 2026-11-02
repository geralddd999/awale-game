#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "display.h"

static SOCKET init_connection(const char *address, int port){
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN sin = { 0 };
	struct hostent *hostinfo;

	if (sock == INVALID_SOCKET){
		perror("socket()");
		exit(errno);
	}

	hostinfo = gethostbyname(address);
	if (hostinfo == NULL){
		fprintf(stderr, "Unknown host %s.\n", address);
		exit(EXIT_FAILURE);
	}

	sin.sin_addr = *(IN_ADDR *)hostinfo->h_addr;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if (connect(sock, (SOCKADDR *)&sin, sizeof(SOCKADDR)) == SOCKET_ERROR){
		perror("connect()");
		exit(errno);
	}
	return sock;
}

/* turns what the user typed into a protocol line: "3" -> "/move 3" in game, plain text -> "/say" */
static void send_input(SOCKET sock, const char *input){
	if (input[0] == '\0') return;
	if (input[0] == '/') send_line(sock, "%s", input);
	else if (display_in_game() && input[0] >= '1' && input[0] <= '6' && input[1] == '\0') send_line(sock, "/move %s", input);
	else send_line(sock, "/say %s", input);
}

static void app(const char *address, int port, const char *name){
	SOCKET sock = init_connection(address, port);
	LineBuf in;
	char buffer[BUF_SIZE];
	int stdin_open = 1;
	fd_set rdfs;

	linebuf_init(&in);
	display_init(name);
	send_line(sock, "/login %s", name);

	while (1){
		FD_ZERO(&rdfs);
		if (stdin_open) FD_SET(STDIN_FILENO, &rdfs);
		FD_SET(sock, &rdfs);

		if (select(sock + 1, &rdfs, NULL, NULL, NULL) == -1){
			perror("select()");
			exit(errno);
		}

		if (stdin_open && FD_ISSET(STDIN_FILENO, &rdfs)){
			if (fgets(buffer, sizeof buffer, stdin) == NULL){
				/* end of input (ctrl-D or piped file): leave politely, wait for the server to close */
				send_line(sock, "/quit");
				stdin_open = 0;
			}
			else {
				buffer[strcspn(buffer, "\r\n")] = '\0';
				send_input(sock, buffer);
			}
		}

		if (FD_ISSET(sock, &rdfs)){
			if (linebuf_fill(&in, sock) == 0){
				printf("Server disconnected !\n");
				break;
			}
			while (linebuf_next(&in, buffer, sizeof buffer)) display_line(buffer);
		}
	}

	closesocket(sock);
}

int main(int argc, char **argv){
	if (argc < 3){
		printf("Usage : %s [address] [username] [port]\n", argv[0]);
		return EXIT_FAILURE;
	}

	app(argv[1], (argc > 3) ? atoi(argv[3]) : PORT, argv[2]);
	return EXIT_SUCCESS;
}
