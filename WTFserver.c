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

struct server_context {
	unsigned int num_connections;
	pthread_mutex_t lock;
};

struct work_args {
	int socket;
	struct server_context *cntx;
};

void *handler (void *args);

int main (int argc, char **argv) {
	/* Check for correct number of arguments */
	if (argc < 2) {
		fprintf(stderr, "ERROR: Need port number.\n");
		return(EXIT_FAILURE);
	} else if (argc > 2) {
		fprintf(stderr, "ERROR: Too many arguments.\n");
		return(EXIT_FAILURE);
	}

	struct server_context *cntx = (struct server_context *) malloc(sizeof(struct server_context));
	cntx->num_connections = 0;
	pthread_mutex_init(&cntx->lock, NULL);

	sigset_t sig;
	sigemptyset(&sig);
	sigaddset(&sig, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &sig, NULL) != 0) {
		fprintf(stderr, "ERROR: Unable to mask SIGPIPE.\n");
		return(EXIT_FAILURE);
	}
	
	/* Get port number */
	int port = atoi(argv[1]);
	
	int server_socket, client_socket;
	pthread_t thread;
	struct addrinfo hints, *res, *p;
	struct sockaddr_storage *client_add;
	int option = 1;
	socklen_t sin_size = sizeof(struct sockaddr_storage);
	struct work_args *wa;
	char *msg = "Hello, socket!";

	/* Initialize hints for getaddrinfo */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, argv[1], &hints, &res) != 0) {
		fprintf(stderr, "ERROR: getaddrinfo() failed.\n");
		pthread_exit(NULL);
	}
	
	for (p = res; p != NULL; p = p->ai_next) {
		/* Create socket */
		server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (server_socket < 0) {
			fprintf(stderr, "ERROR: Could not open socket.\n");
			continue;
		}

		/* Make address resuable */
		if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0) {
			fprintf(stderr, "ERROR: Socket setsockopt() failed.\n");
			close(server_socket);
			continue;
		}

		/* Bind socket to address*/
		if (bind(server_socket, p->ai_addr, p->ai_addrlen) < 0) {
			fprintf(stderr, "ERROR: Socket bind() failed.\n");
			close(server_socket);
			continue;
		}

		/* Listen */
		if (listen(server_socket, 20) < 0) {
			fprintf(stderr, "ERROR: Socket listen() failed.\n");
			close(server_socket);
			continue;
		}
		
		break;
	}
	
	freeaddrinfo(res);

	if (p == NULL) {
		fprintf(stderr, "ERROR: Could not bind to any socket.\n");
		pthread_exit(NULL);
	}

	while(1) {
		client_add = malloc(sin_size);
		/* Accept */
		if ((client_socket = accept(server_socket, (struct sockaddr *) &client_add, &sin_size)) < 0) {
			fprintf(stderr, "ERROR: Could not accept connection.\n");
			free(client_add);
			continue;
		}
		
		wa = malloc(sizeof(struct work_args));
		wa->socket = client_socket;
		wa->cntx = cntx;

		if(pthread_create(&thread, NULL, handler, wa) != 0) {
			fprintf(stderr, "ERROR: Could not create thread.\n");
			free(client_add);
			free(wa);
			close(client_socket);
			close(server_socket);
			return EXIT_FAILURE;
		}
	}
	
	pthread_mutex_destroy(&cntx->lock);
	free(cntx);
	return EXIT_SUCCESS;
}

void *handler(void *args) {
	struct work_args *wa;
	struct server_context *cntx;
	int socket, sent, i;
	char sending[100];

	wa = (struct work_args *) args;
	socket = wa->socket;
	cntx = wa->cntx;

	pthread_detach(pthread_self());

	printf("Socket %d connected.\n", socket);

	while(1) {
		sprintf(sending, "Hello socket! It is %d.\n", (int) time(NULL));

		sent = send(socket, sending, strlen(sending), 0);
		if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
			printf("Socket %d disconnected.\n", socket);
			close(socket);
			free(wa);
			pthread_exit(NULL);
		} else if (sent < 0) {
			fprintf(stderr, "ERROR: Something went wrong while sending.\n");
			free(wa);
			pthread_exit(NULL);
		}
		sleep(5);
	}
	
	pthread_exit(NULL);
}

