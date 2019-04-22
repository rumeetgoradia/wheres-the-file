#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

int main (int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "ERROR: Need port number.\n");
		return(EXIT_FAILURE);
	} else if (argc > 2) {
		fprintf(stderr, "ERROR: Too many arguments.\n");
		return(EXIT_FAILURE);
	}
	int port = atoi(argv[1]);
	int server_socket, client_socket;
	struct sockaddr_in server_add, client_add;
	int option = 1;
	socklen_t sin_size = sizeof(struct sockaddr_in);
	char *msg = "Hello, socket!";

	/* Initialize socket address */
	memset(&server_add, 0, sizeof(server_add));
	server_add.sin_family = AF_INET;
	server_add.sin_port = htons(port);
	server_add.sin_addr.s_addr = INADDR_ANY;

	/* Create socket */
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		fprintf(stderr, "ERROR: Could not open socket.\n");
		exit(EXIT_FAILURE);
	}

	/* Make address resuable */
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0) {
		fprintf(stderr, "ERROR: Socket setsockopt() failed.\n");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	/* Bind socket to address*/
	if (bind(server_socket, (struct sockaddr *) &server_add, sizeof(server_add)) < 0) {
		fprintf(stderr, "ERROR: Socket bind() failed.\n");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	/* Listen */
	if (listen(server_socket, 20) < 0) {
		fprintf(stderr, "ERROR: Socket listen() failed.\n");
		close(server_socket);
		exit(EXIT_FAILURE);
	}
	printf("Waiting for a connection...\n");

	/* Accept */
	if ((client_socket = accept(server_socket, (struct sockaddr *) &client_add, &sin_size)) < 0) {
		fprintf(stderr, "ERROR: Socket accept() failed.\n");
		close(server_socket);
		exit(EXIT_FAILURE);
	}
	
	/*Send a message */
	if (send(client_socket, msg, strlen(msg), 0) <= 0) {
		fprintf(stderr, "ERROR: Socket send() failed.\n");
		close(server_socket);
		close(client_socket);
		exit(EXIT_FAILURE);
	}

	close(client_socket);
	printf("Message sent.\n");
	close(server_socket);
	return EXIT_SUCCESS;
}

