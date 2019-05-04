#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include "helperfunctions.h"

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
	if (port < 1024) {
		fprintf(stderr, "ERROR: Invalid port number.\n");
		return EXIT_FAILURE;
	}
	
	int server_socket, client_socket;
	pthread_t thread;
	struct addrinfo hints, *res, *p;
	struct sockaddr_storage *client_add;
	int option = 1;
	socklen_t sin_size = sizeof(struct sockaddr_storage);
	struct work_args *wa;

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
	
	/* Create server directory if doesn't already exist */
	struct stat st = {0};
	if (stat("./.server_directory", &st) == -1) {
		mkdir("./.server_directory", 0744);
	}

	printf("Waiting for client...\n");
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
	int client_socket, sent, received, i;	
	char recv_buffer[BUFSIZ];

	wa = (struct work_args *) args;
	client_socket = wa->socket;
	cntx = wa->cntx;

	pthread_detach(pthread_self());

	printf("Socket %d connected.\n", client_socket);
	
	received = recv(client_socket, recv_buffer, BUFSIZ - 1, 0);
	recv_buffer[received] = '\0';
	if (received <= 0) {
		fprintf(stderr, "ERROR: Server-side recv() failed.\n");
		pthread_exit(NULL);
	}
	char *token;
	token = strtok(recv_buffer, ":");

	if (token[0] == 'c') {

		token = strtok(NULL, ":");

		char *new_proj_path = (char *) malloc(strlen(token) + 22);
		if (token[strlen(token) - 1] != '/') {
			snprintf(new_proj_path, strlen(token) + 22, "./.server_directory/%s/", token);
		} else {
			snprintf(new_proj_path, strlen(token) + 22, "./.server_directory/%s", token);
		}
		char sending[2];
/*		struct stat st = {0};
		if (stat(new_proj_path, &st) == -1) { */
		if (check_dir(new_proj_path) == -1) {
			mkdir(new_proj_path, 0744);
			char *new_mani_path = (char *) malloc(strlen(new_proj_path) + 11);
			snprintf(new_mani_path, strlen(new_proj_path) + 11, "%s.Manifest", new_proj_path);
			int fd_mani = open(new_mani_path, O_CREAT | O_WRONLY, 0744);
			write(fd_mani, "0\n", 2);
			close(fd_mani);
			free(new_mani_path);
			free(new_proj_path);
			sending[0] = 'c';
			sent = send(client_socket, sending, 2, 0);
			printf("Creation of project \"%s\" successful.\n", token);
			pthread_exit(NULL);
		} else {
			sending[0] = 'x';
			sent = send(client_socket, sending, 2, 0);
			free(new_proj_path);
			fprintf(stderr, "ERROR: Creation of project \"%s\" failed.\n", token);
			pthread_exit(NULL);
		}
	} else if (token[0] == 'd') {
		token = strtok(NULL, ":");
		char *proj_path = (char *) malloc(strlen(token) + 22);
		snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
		char sending[2];
		if (check_dir(proj_path) == -1) {
			fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", token);
			sending[0] = 'x';
			sent = send(client_socket, sending, 2, 0);
			free(proj_path);
			pthread_exit(NULL);	
		} else {
			int check = remove_dir(proj_path);	
			if (check == 0) {
				sending[0] = 'g';
				sent = send(client_socket, sending, 2, 0);
				free(proj_path);
				pthread_exit(NULL);
			} else {
				sending[0] = 'b';
				sent = send(client_socket, sending, 2, 0);
				fprintf(stderr, "ERROR: Could not remove \"%s\" project from server.\n", token);
				free(proj_path);
				pthread_exit(NULL);
			}
		}
	} else if (token[0] == 'v') {
		token = strtok(NULL, ":");
		char *proj_path = (char *) malloc(strlen(token) + 22);
		snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
		if (check_dir(proj_path) == -1) {
			char sending[2] = "b";
			sent = send(client_socket, sending, 2, 0);
			fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", token);
			free(proj_path);
			pthread_exit(NULL);
		}
		free(proj_path);
		char *mani_path = (char *) malloc(strlen(token) + 31);
		snprintf(mani_path, strlen(token) + 31, ".server_directory/%s/.Manifest", token);
		int fd_mani = open(mani_path, O_RDONLY);
		if (fd_mani < 0) {
			fprintf(stderr, "ERROR: Unable to open \".Manifest\" file for \"%s\" project.\n", token);
			free(mani_path);
			free(proj_path);
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			pthread_exit(NULL);	
		}
		struct stat st = {0};
		if (fstat(fd_mani, &st) < 0) {
			fprintf(stderr, "ERROR: fstat() failed.\n");
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			free(mani_path);

			pthread_exit(NULL);

		}
		char file_size[256];
		snprintf(file_size, 256, "%d", st.st_size);
		sent = send(client_socket, file_size, 256, 0);	
		if (sent < 0) {
			fprintf(stderr, "ERROR: Could not send size of \"%s\".\n", mani_path);
			free(mani_path);
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			pthread_exit(NULL);	
		}
		char contents[st.st_size + 1];
		int bytes_read = read(fd_mani, contents, st.st_size);
		contents[bytes_read] = '\0';
		sent = send(client_socket, contents, bytes_read, 0);
		/* off_t offset = 0;
		int remaining = st.st_size;
		while (((sent = sendclient_(socket, (contents + offset), st.st_size, 0)) > 0) && (remaining > 0)) {
			printf("sending: %s", contents+offset);
			remaining -= sent;
			offset += sent;
		} */
		printf("Sent \".Manifest\" file for \"%s\" project to client.\n", token);
		free(mani_path);
		pthread_exit(NULL);
	} else if (token[0] == 'o') {
		token = strtok(NULL, ":");
		char *proj_path = (char *) malloc(strlen(token) + 22);
		snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
		if (check_dir(proj_path) == -1) {
			char sending[2] = "b";
			sent = send(client_socket, sending, 2, 0);
			free(proj_path);
			fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", token);
			pthread_exit(NULL);
		}
		free(proj_path);
		char *mani_path = (char *) malloc(strlen(token) + 31);
		snprintf(mani_path, strlen(token) + 31, ".server_directory/%s/.Manifest", token);
		int fd_mani = open(mani_path, O_RDONLY);
		if (fd_mani < 0) {
			fprintf(stderr, "ERROR: Unable to open \".Manifest\" file for \"%s\" project.\n", token);
			free(mani_path);
			free(proj_path);
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			pthread_exit(NULL);	
		}
		struct stat st = {0};
		if (fstat(fd_mani, &st) < 0) {
			fprintf(stderr, "ERROR: fstat() failed.\n");
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			free(mani_path);
			pthread_exit(NULL);

		}
		char file_size[256];
		snprintf(file_size, 256, "%d", st.st_size);
		sent = send(client_socket, file_size, 256, 0);	
		if (sent < 0) {
			fprintf(stderr, "ERROR: Could not send size of \"%s\".\n", mani_path);
			free(mani_path);
			char sending[2] = "x";
			sent = send(client_socket, sending, 2, 0);
			pthread_exit(NULL);	
		}
		char contents[st.st_size + 1];
		int bytes_read = read(fd_mani, contents, st.st_size);
		contents[bytes_read] = '\0';
		sent = send(client_socket, contents, bytes_read, 0);
		char *buffer = (char *) malloc(BUFSIZ);
		received = recv(client_socket, buffer, BUFSIZ, 0);
		int remaining = atoi(buffer);
		received = recv(client_socket, buffer, BUFSIZ, 0);
		char temp[remaining + 1];
		strcpy(temp, buffer);
		char *vers_token = strtok(temp, "\n");
		int version = atoi(vers_token);
		char *comm_path = (char *) malloc(strlen(token) + 28 + sizeof(version));
		snprintf(comm_path, strlen(token) + 28 + sizeof(version), ".server_directory/%s/.Commit%d", token, version);
		int fd_comm_server = open(comm_path, O_CREAT | O_RDWR | O_APPEND, 0744);
		char sending[2] = "g";
		if (fd_comm_server < 0) {
			fprintf(stderr, "ERROR: Unable to create \".Commit%d\" file for \"%s\" project.\n", version, token);
			free(comm_path);
			sending[0] = 'd';
			sent = send(client_socket, sending, 2, 0);
			free(buffer);
			pthread_exit(NULL);
		}
		sent = send(client_socket, sending, 2, 0);
		free(comm_path);
		write(fd_comm_server, buffer, strlen(buffer));
		free(buffer);
		close(fd_comm_server);
	}
	
	pthread_exit(NULL);
}

