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
/* lol */
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
			char *new_vers_path = (char *) malloc(strlen(new_proj_path) + 9);
			snprintf(new_vers_path, strlen(new_proj_path) + 9, "%sversion0", new_proj_path);
			mkdir(new_vers_path, 0744);
			free(new_vers_path);
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
		printf("Sent .Manifest file for \"%s\" project to client.\n", token);
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
		char *to_send = (char *) malloc(2);
		int fd_mani = open(mani_path, O_RDONLY);
		int mani_size = get_file_size(fd_mani);
		if (fd_mani < 0 || mani_size < 0) {
			fprintf(stderr, "ERROR: Unable to open \".Manifest\" file for \"%s\" project.\n", token);
			free(mani_path);
			snprintf(to_send, 2, "x");
			sent = send(client_socket, to_send, 2, 0);
			pthread_exit(NULL);	
		}
		int sending_size = sizeof(mani_size);
		free(to_send);
		to_send = (char *) malloc(sending_size + 1);
		snprintf(to_send, sending_size, "%d", mani_size);
		sent = send(client_socket, to_send, sending_size, 0);		
		free(to_send);
		sending_size = mani_size;
		to_send = (char *) malloc(sending_size + 1);
		int bytes_read = read(fd_mani, to_send, sending_size);
		sent = send(client_socket, to_send, bytes_read, 0);
		while (sent < bytes_read) {
			int bytes_sent = send(client_socket, to_send + sent, bytes_read, 0);
			sent += bytes_sent;
		} 
		char mani_input[bytes_read + 1];
		strcpy(mani_input, to_send);
		/* Get .Commit data */
		char *recving = (char *) malloc(sizeof(int));
		received = recv(client_socket, recving, sizeof(int), 0);
		if (recving[0] == 'x') {
			fprintf(stderr, "Client failed to create new .Commit for project \"%s\".\n", token);
			free(recving);
			free(mani_path);
			free(to_send);
			close(fd_mani);
			pthread_exit(NULL);
		} else if (recving[0] == 'b') {
			fprintf(stderr, "Client's copy of project \"%s\" is not up-to-date.\n", token);
			free(recving);
                        free(mani_path);
                        free(to_send);
                        close(fd_mani);
			pthread_exit(NULL);
		}
		int comm_size = atoi(recving);
		if (comm_size == 0) {
			fprintf(stderr, "ERROR: Empty .Commit sent from client for project \"%s\".", token);
			close(fd_mani);
			free(recving);
			free(to_send);
			pthread_exit(NULL);
		}
		free(recving);
		recving = (char *) malloc(comm_size + 1);
		received = recv(client_socket, recving, comm_size, 0);

//		char temp[remaining + 1];
//		strcpy(temp, buffer);
		srand(time(0));
		int version = rand() % 10000;

		char *comm_path = (char *) malloc(strlen(token) + 28 + sizeof(version));
		snprintf(comm_path, strlen(token) + 28 + sizeof(version), ".server_directory/%s/.Commit%d", token, version);
		int fd_comm_server = open(comm_path, O_CREAT | O_RDWR | O_APPEND, 0744);
		free(to_send);
		to_send = (char *) malloc(2);
		if (fd_comm_server < 0) {
			fprintf(stderr, "ERROR: Unable to create .Commit%d for \"%s\" project.\n", version, token);
			free(comm_path);
			snprintf(to_send, 2, "b");
			sent = send(client_socket, to_send, 2, 0);
			free(recving);
			free(to_send);
			close(fd_comm_server);
			pthread_exit(NULL);
		}
		free(comm_path);
		write(fd_comm_server, recving, received);
		snprintf(to_send, 2, "g");
		sent = send(client_socket, to_send, 2, 0);
		free(recving);
		free(to_send);
		close(fd_comm_server);
		close(fd_mani);
		printf("Commit successful.\n");
	} else if (token[0] == 'p') {
		token = strtok(NULL, ":");
		char project[strlen(token) + 1];
		strcpy(project, token);
		char *to_send = (char *) malloc(2);
		if (check_dir(project) == -1) {
			snprintf(to_send, 2, "b");
		} else {
			snprintf(to_send, 2, "g");
		}
		sent = send(client_socket, to_send, 2, 0);
		char *recving = (char *) malloc(256);
		received = recv(client_socket, recving, 256, 0);
		int size = atoi(recving);
		free (recving);
		recving = (char *) malloc(size + 1);
		received = recv(client_socket, recving, size, 0);
		while (received < size) {
			int bytes_recv = recv(client_socket, recving + received, size, 0);
			received += bytes_recv;
		}
		recving[received] = '\0';
		/* Got the client's commit */
		char comm_input[strlen(recving)];
		strcpy(comm_input, recving);
		int comm_check = push_check(project, comm_input);
		if (comm_check == -1) {
			free(recving);
			snprintf(to_send, 2, "x");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			pthread_exit(NULL);
		} else if (comm_check == 1) {
			free(recving);
			fprintf(stderr, "ERROR: Could not find matching .Commit for project \"%s\".\n", project);
			snprintf(to_send, 2, "b");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			pthread_exit(NULL);
		}
		char mani_path[strlen(project) + 31];
		snprintf(mani_path, strlen(project) + 31, ".server_directory/%s/.Manifest", project);
		int fd_mani = open(mani_path, O_RDWR);
		if (fd_mani < 0) {
			free(recving);
			snprintf(to_send, 2, "x");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			fprintf(stderr, "ERROR: Failed to open .Manifest for project \"%s\".\n", project);
			pthread_exit(NULL);
		}
		int mani_size = get_file_size(fd_mani);
		if (mani_size < 0) {
			free(recving);
			snprintf(to_send, 2, "x");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			fprintf(stderr, "ERROR: Failed to get .Manifest's size for project \"%s\".\n", project);
			pthread_exit(NULL);
		}
		snprintf(to_send, 2, "g");
		sent = send(client_socket, to_send, 2, 0);
		char mani_buff[mani_size + 1];
		int bytes_read = read(fd_mani, mani_buff, mani_size);
		char mani_jic[mani_size + 1];
		char *new_mani_buff = (char *) malloc(mani_size + 1);
		strncpy(mani_jic, mani_buff, bytes_read);
/*		mani_jic[bytes_read - 1] = '\0'; */
		strncpy(new_mani_buff, mani_buff, bytes_read);
/*		new_mani_buff[bytes_read - 1] = '\0'; */
		char *mani_token = strtok(mani_buff, "\n");
		int version = atoi(mani_token);	
		new_mani_buff += strlen(mani_token) + 1;
		char *write_to_new_mani = malloc(strlen(new_mani_buff) + 2 + sizeof(version + 1));
		snprintf(write_to_new_mani, strlen(new_mani_buff) + 2 + sizeof(version + 1), "%d\n%s", version + 1, new_mani_buff);
		fd_mani = open(mani_path, O_RDWR | O_TRUNC);
		write(fd_mani, write_to_new_mani, strlen(write_to_new_mani));
		char vers_path[strlen(project) + 29 + sizeof(version)];
		snprintf(vers_path, strlen(project) + 29 + sizeof(version), ".server_directory/%s/version%d", project, version);
		char new_vers_path[strlen(project) + 29 + sizeof(version + 1)];
		snprintf(new_vers_path, strlen(project) + 29 + sizeof(version + 1), ".server_directory/%s/version%d", project, version + 1);
		mkdir(new_vers_path, 0744);
		int dir_copy_check = dir_copy(vers_path, new_vers_path);
		if (dir_copy_check == 0) {
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);
		} else {
			snprintf(to_send, 2, "b");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			free(recving);
			write(fd_mani, mani_jic, mani_size);
			remove_dir(new_vers_path);
			fprintf(stderr, "ERROR: Failed to instantiate new version of project \"%s\".\n", project);
			pthread_exit(NULL);
		}	
		/* Tokenizing comm_input */
		int count = 0;
		char *comm_token;
		int j = 0, k = 0;
		int last_sep = 0;
		int token_len = 0;
		int len = strlen(comm_input);
		int delete_check = 0, modify_check = 0;
		char *file_path = NULL;
		for (j = 0; j < len; ++j) {
			if (comm_input[j] != '\t' && comm_input[j] != '\n') {
				++token_len;
				continue;
			} else {
				comm_token = (char *) malloc(token_len + 1);
				for (k = 0; k < token_len; ++k) {
					comm_token[k] = comm_input[last_sep + k];
				}
				comm_token[token_len] = '\0';
				last_sep += token_len + 1;
				token_len = 0;
				++count;
			}
			if (count % 4 == 1) {
				if (comm_token[0] == 'D') {
					delete_check = 1;
				} else if (comm_token[0] == 'M') {
					modify_check = 1;
				}
				free(comm_token);
			} else if (count % 4 == 2) {
				free(comm_token);	
			} else if (count % 4 == 3) {
				file_path = (char *) malloc(strlen(comm_token) + 1);
				strncpy(file_path, comm_token, strlen(comm_token));
				comm_token += strlen(project) + 1;
				int path_len = strlen(new_vers_path) + 1 + strlen(file_path);
				char new_file_path[path_len + 1];
				snprintf(new_file_path, path_len, "%s/%s", new_vers_path, comm_token);
				if (delete_check == 1) {
					remove(new_file_path);
					remover(fd_mani, comm_token, write_to_new_mani);
				} else {
//					free(recving);
					recving = (char *) malloc(sizeof(int));
					received = recv(client_socket, recving, sizeof(int), 0);
					if (recving[0] == 'x') {
						fprintf(stderr, "ERROR: Client could not send coneents of file.\n");
						fd_mani = open(mani_path, O_WRONLY | O_TRUNC);
						write(fd_mani, mani_jic, mani_size);
						remove_dir(new_vers_path);
						free(file_path);
						free(comm_token);
						pthread_exit(NULL);
					}
					int file_size = atoi(recving);
					free(recving);
					recving = (char *) malloc(file_size + 1);
					received = recv(client_socket, recving, file_size, 0);
					while (received < file_size) {
						int bytes_recved = recv(client_socket, recving + received, file_size, 0);
						received += bytes_recved;
					}
					int fd_new_file;
					if (modify_check) {
						fd_new_file = open(new_file_path, O_WRONLY | O_TRUNC);
					} else {
						fd_new_file = open(new_file_path, O_WRONLY | O_CREAT, 0744);
					}
					if (fd_new_file < 0) {
						free(recving);
						snprintf(to_send, 2, "x");
						sent = send(client_socket, to_send, 2, 0);
						free(to_send);
						fprintf(stderr, "ERROR: Failed to open \"%s\" filein project.\n", new_file_path);
						fd_mani = open(mani_path, O_RDWR | O_TRUNC);
						write(fd_mani, mani_jic, mani_size);
						close(fd_mani);
						close(fd_new_file);
						free(comm_token);
						pthread_exit(NULL);
					}
					write(fd_new_file, recving, file_size);
					snprintf(to_send, 2, "g");
					sent = send(client_socket, to_send, 2, 0);
//					free(comm_token);
				}
			} else if (count % 4 == 0) {
				if (!delete_check) {
					add(fd_mani, comm_token, file_path, write_to_new_mani, 1);
					int new_mani_size = get_file_size(fd_mani);
					free(write_to_new_mani);
					write_to_new_mani = (char *) malloc(new_mani_size + 1);
					lseek(fd_mani, 0, 0);
					int br = read(fd_mani, write_to_new_mani, new_mani_size);
					printf("new mani size: %d, br: %d\n", new_mani_size, br);
					write_to_new_mani[br] = '\0';
					printf("%s\n", write_to_new_mani);
				} else {
					delete_check = 0;
				}	
				free(comm_token);
				free(file_path);
			}
		}
		if (token_len > 0) {
			comm_token = (char *) malloc(token_len + 1);
			for (j = 0; j < token_len; ++j) {
				comm_token[j] = comm_input[last_sep + j];
			}
			comm_token[token_len] = '\0';
			if (!delete_check) {
				add(fd_mani, comm_token, file_path, write_to_new_mani, 1);
			}
			free(comm_token);
			free(file_path);
		}
		printf("Push complete!\n");
		snprintf(to_send, 2, "g");
		sent = send(client_socket, to_send, 2, 0);	
	}
	pthread_exit(NULL);
}

