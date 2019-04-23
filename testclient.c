#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv) {
	int client_socket;
	struct addrinfo hints, *res, *p;
	char *host, *port;
	char buffer[100 + 1];
	int received;
	int option;
	/* This will be host and port in .configure */
/*	while ((option = getopt(argc, argv, "h:p:")) != -1) {
		switch (option) {
			case 'h':
				host = strdup(optarg);
				break;
			case 'p':
				port = strdup(optarg);
				break;
			default:
				fprintf(stderr, "ERROR: Unknown option.\n");
				exit(EXIT_FAILURE);
		}
	}
	if (host == NULL || port == NULL) {
		fprintf(stderr, "USAGE: client -h HOST -p PORT\n");
		exit(1);
	} */
	/* Setup hints */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo("127.0.0.1", "5050", &hints, &res) != 0) {
		fprintf(stderr, "ERROR: getaddrinfo() failed.\n");
		exit(EXIT_FAILURE);
	}
	for (p = res; p != NULL; p = p->ai_next) {
		if ((client_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			fprintf(stderr, "ERROR: Could not open socket.\n");
			continue;
		}
		if (connect(client_socket, p->ai_addr, p->ai_addrlen) < 0) {
			close(client_socket);
			fprintf(stderr, "ERROR: Could not connect to socket.\n");
			continue;
		}
		break;
	}
	freeaddrinfo(res);
	received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
	while(1) {
		if (received == 0) {
			fprintf(stderr, "ERROR: Server closed the connection.\n");
			close(client_socket);
			exit(EXIT_FAILURE);
		} else if (received < 0) {
			fprintf(stderr, "ERROR: Socket recv() failed.\n");
			close(client_socket);
			exit(EXIT_FAILURE);
		} else {
			buffer[received] = '\0';
			printf("Received:\n%s\n", buffer);
			received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
		}
	}
	close(client_socket);
	return EXIT_SUCCESS;
}
