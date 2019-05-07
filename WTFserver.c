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

static int keep_running = 1;

struct server_context {
	pthread_mutex_t lock;
};

struct work_args {
	int socket;
	struct server_context *cntx;
};

void *thread_handler (void *args);

void int_handler() {
	keep_running = 0;
}

int main (int argc, char **argv) {
	/* Check for correct number of arguments */
	if (argc < 2) {
		fprintf(stderr, "ERROR: Need port number.\n");
		return(EXIT_FAILURE);
	} else if (argc > 2) {
		fprintf(stderr, "ERROR: Too many arguments.\n");
		return(EXIT_FAILURE);
	}
	struct sigaction act;
	act.sa_handler = int_handler;
	sigaction(SIGINT, &act, NULL);
	while (keep_running) {
		struct server_context *cntx = (struct server_context *) malloc(sizeof(struct server_context));

		pthread_mutex_init(&cntx->lock, NULL);
		/* Ignore SIGPIPE --> "  If client closes connection, SIGPIPE signal produced --> Process killed --> Ignore signal */
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
		while(1 & keep_running) {
			sigaction(SIGINT, &act, NULL);
			client_add = malloc(sin_size);
			/* Accept */
			if ((client_socket = accept(server_socket, (struct sockaddr *) &client_add, &sin_size)) < 0 && keep_running) {
				fprintf(stderr, "ERROR: Could not accept connection.\n");
				free(client_add);
				continue;
			}

			wa = malloc(sizeof(struct work_args));
			wa->socket = client_socket;
			wa->cntx = cntx;
			while (keep_running) {
				if(pthread_create(&thread, NULL, thread_handler, wa) != 0) {
					fprintf(stderr, "ERROR: Could not create thread.\n");
					free(client_add);
					free(wa);
					close(client_socket);
					close(server_socket);
					return EXIT_FAILURE;
				}
			}
		}
		pthread_mutex_destroy(&cntx->lock);
		free(cntx);
	}
	printf("Server shutting down.\n");
	return EXIT_SUCCESS;
}

void *thread_handler(void *args) {
	struct sigaction act;
	act.sa_handler = int_handler;
	sigaction(SIGINT, &act, NULL);
	
	while (keep_running) {
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
			fprintf(stderr, "ERROR: Server's recv() failed.\n");
			pthread_exit(NULL);
		}
		char *token;
		token = strtok(recv_buffer, ":");
		/* One mutex lock per thread */
		pthread_mutex_lock(&cntx->lock);
		/* Do certain WTF function based on single-char command sent from client */
		if (token[0] == 'c') {
			/* CREATE */
			token = strtok(NULL, ":");
			/* Set up project path with regard to .server_directory; basic start for every command */
			char *new_proj_path = (char *) malloc(strlen(token) + 22);
			if (token[strlen(token) - 1] != '/') {
				snprintf(new_proj_path, strlen(token) + 22, "./.server_directory/%s/", token);
			} else {
				snprintf(new_proj_path, strlen(token) + 22, "./.server_directory/%s", token);
			}
			char sending[2];
			/* If dir doesn't already exist, create it; else, send error char */
			if (check_dir(new_proj_path) == -1) {
				mkdir(new_proj_path, 0744);
				char *new_vers_path = (char *) malloc(strlen(new_proj_path) + 9);
				snprintf(new_vers_path, strlen(new_proj_path) + 9, "%sversion0", new_proj_path);
				mkdir(new_vers_path, 0744);
				/* Set up version's own .Manifest, for rollback purposes */
				char extra_mani[strlen(new_vers_path) + 11];
				snprintf(extra_mani, strlen(new_vers_path) + 11, "%s/.Manifest", new_vers_path);
				int fd_extra = open(extra_mani, O_CREAT | O_WRONLY, 0744);
				write(fd_extra, "0\n", 2);
				close(fd_extra);
				free(new_vers_path);
				/* Set up project-wide .Manifest */
				char *new_mani_path = (char *) malloc(strlen(new_proj_path) + 11);
				snprintf(new_mani_path, strlen(new_proj_path) + 11, "%s.Manifest", new_proj_path);
				int fd_mani = open(new_mani_path, O_CREAT | O_WRONLY, 0744);
				write(fd_mani, "0\n", 2);
				close(fd_mani);
				/* Set up project-wide .History */
				char hist_path[strlen(new_proj_path) + 10];
				snprintf(hist_path, strlen(new_proj_path) + 10, "%s.History", new_proj_path);
				int fd_hist = open(hist_path, O_CREAT | O_WRONLY, 0744);
				write(fd_hist, "create\n0\n\n", 10);
				close(fd_hist);
				free(new_mani_path);
				free(new_proj_path);
				sending[0] = 'c';
				sent = send(client_socket, sending, 2, 0);
				printf("Creation of project \"%s\" successful.\n", token);
			} else {
				/* On failure or success, always send a specific char; client will recognize these chars to mean success or different types
				* of failure. */
				sending[0] = 'x';
				sent = send(client_socket, sending, 2, 0);
				free(new_proj_path);
				fprintf(stderr, "ERROR: Creation of project \"%s\" failed.\n", token);
				pthread_exit(NULL);
			}
		} else if (token[0] == 'd') {
			/* DESTROY */
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
				/* Project exists, use helper function to get rid of it */
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
			/* CURRENTVERSION */
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
			/* Get server's copy of .Manifest for project */
			char *mani_path = (char *) malloc(strlen(token) + 31);
			snprintf(mani_path, strlen(token) + 31, ".server_directory/%s/.Manifest", token);
			int fd_mani = open(mani_path, O_RDONLY);
			if (fd_mani < 0) {
				fprintf(stderr, "ERROR: Unable to open \".Manifest\" file for \"%s\" project.\n", token);
				free(mani_path);
				free(proj_path);
				char sending[2] = "x";
				sent = send(client_socket, sending, 2, 0);	
				close(fd_mani);
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
			char *contents = malloc(st.st_size + 1);
			int bytes_read = read(fd_mani, contents, st.st_size);
			contents[bytes_read] = '\0';
			sent = send(client_socket, contents, bytes_read, 0);
			while (sent < bytes_read) {
				int bs = send(client_socket, contents + sent, bytes_read, 0);
				sent += bs;
			}
			/* off_t offset = 0;
			int remaining = st.st_size;
			while (((sent = sendclient_(socket, (contents + offset), st.st_size, 0)) > 0) && (remaining > 0)) {
				printf("sending: %s", contents+offset);
				remaining -= sent;
				offset += sent;
			} */
			printf("Sent .Manifest file for \"%s\" project to client.\n", token);
			close(fd_mani);
			free(mani_path);
			pthread_exit(NULL);
		} else if (token[0] == 'o') {
			/* COMMIT */
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
			/* Get .Manifest */
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
				close(fd_mani);
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
				fprintf(stderr, "ERROR: Empty .Commit sent from client for project \"%s\".\n", token);
				close(fd_mani);
				free(recving);
				free(to_send);
				pthread_exit(NULL);
			}
			free(recving);
			recving = (char *) malloc(comm_size + 1);
			received = recv(client_socket, recving, comm_size, 0);
			/* Create random version number to differentiate .Commits */
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
				close(fd_mani);
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
			/* PUSH */
			token = strtok(NULL, ":");
			char project[strlen(token) + 1];
			strcpy(project, token);
			project[strlen(token)] = '\0';
			char *proj_path = (char *) malloc(strlen(token) + 22);
			snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
			char *to_send = (char *) malloc(2);
			if (check_dir(proj_path) == -1) {
				snprintf(to_send, 2, "b");
			} else {
				snprintf(to_send, 2, "g");
			}
			sent = send(client_socket, to_send, 2, 0);
			char *recving = (char *) malloc(256);
			received = recv(client_socket, recving, 256, 0);
			int size = atoi(recving);
			if (size == 0) {
				fprintf(stderr, "ERROR: Client's .Commit is empty for project \"%s\".\n", token);
				free(recving);
				free(to_send);
				pthread_exit(NULL);
			}
			free(recving);
			recving = (char *) malloc(size + 1);
			received = recv(client_socket, recving, size, 0);
			while (received < size) {
				int bytes_recv = recv(client_socket, recving + received, size, 0);
				received += bytes_recv;
			}
			recving[received] = '\0';
			printf("got comm input\n");
			/* Get the client's commit */
			char comm_input[strlen(recving) + 1];
			strcpy(comm_input, recving);
			comm_input[strlen(recving)] = '\0';
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
			/* Open server's .Manifest */
			char mani_path[strlen(project) + 31];
			snprintf(mani_path, strlen(project) + 31, ".server_directory/%s/.Manifest", project);
			int fd_mani = open(mani_path, O_RDWR);
			if (fd_mani < 0) {
				free(recving);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				fprintf(stderr, "ERROR: Failed to open .Manifest for project \"%s\".\n", project);
				close(fd_mani);
				pthread_exit(NULL);
			}
			int mani_size = get_file_size(fd_mani);
			if (mani_size < 0) {
				free(recving);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				fprintf(stderr, "ERROR: Failed to get .Manifest's size for project \"%s\".\n", project);
				close(fd_mani);
				pthread_exit(NULL);
			}
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);
			/* Set up a copy of the old .Manifest in case failure and a new .Manifest with updated version number */
			char mani_buff[mani_size + 1];
			int bytes_read = read(fd_mani, mani_buff, mani_size);
			char mani_jic[mani_size + 1];
			char *new_mani_buff = (char *) malloc(mani_size + 1);
			strncpy(mani_jic, mani_buff, bytes_read);
			strncpy(new_mani_buff, mani_buff, bytes_read);
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
			/* Make new copy of directory */
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
				close(fd_mani);
				pthread_exit(NULL);
			}	
			/* Tokenizing .Commit's input */
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
						/* If D, get rid of file and mark it deleted in .Manifest */
						remove(new_file_path);	
						remover(fd_mani, file_path, write_to_new_mani);
						/* Have to update .Manifest input everytime new change is made */
						int new_mani_size = get_file_size(fd_mani);
						free(write_to_new_mani);
						write_to_new_mani = (char *) malloc(new_mani_size + 1);
						lseek(fd_mani, 0, 0);
						int br = read(fd_mani, write_to_new_mani, new_mani_size);
						write_to_new_mani[br] = '\0';
					} else {
						free(recving);
						/* Get input of files marked A or M from client and put it in respective files */
						recving = (char *) malloc(sizeof(int));
						received = recv(client_socket, recving, sizeof(int), 0);
						if (recving[0] == 'x') {
							fprintf(stderr, "ERROR: Client could not send coneents of file.\n");
							/* On failure, revert .Manifest to old version */
							fd_mani = open(mani_path, O_WRONLY | O_TRUNC);
							write(fd_mani, mani_jic, mani_size);
							remove_dir(new_vers_path);
							free(file_path);
							free(comm_token);
							close(fd_mani);
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
						close(fd_new_file);
					}
				} else if (count % 4 == 0) {
					if (!delete_check) {
						/* Update .Manifest with new version or new entry */
						add(fd_mani, comm_token, file_path, write_to_new_mani, 1);
						int new_mani_size = get_file_size(fd_mani);
						free(write_to_new_mani);
						write_to_new_mani = (char *) malloc(new_mani_size + 1);
						lseek(fd_mani, 0, 0);
						int br = read(fd_mani, write_to_new_mani, new_mani_size);
						write_to_new_mani[br] = '\0';
						modify_check = 0;
					} else {
						delete_check = 0;
					}	
					free(comm_token);
					free(file_path);
				}
			}
			/* For loop finishes before last hash is recognized, but only put in if not a delete */
			if (token_len > 0 && !delete_check) {
				comm_token = (char *) malloc(token_len + 1);
				for (j = 0; j < token_len; ++j) {
					comm_token[j] = comm_input[last_sep + j];
				}
				comm_token[token_len] = '\0';
				if (!delete_check) {
					add(fd_mani, comm_token, file_path, write_to_new_mani, 1);
					int new_mani_size = get_file_size(fd_mani);
					free(write_to_new_mani);
					write_to_new_mani = (char *) malloc(new_mani_size + 1);
					lseek(fd_mani, 0, 0);
					int br = read(fd_mani, write_to_new_mani, new_mani_size);
					write_to_new_mani[br] = '\0';
				}
				free(comm_token);
				free(file_path);
			}
			/* Save current .Manifest in version-specific .Manifest, for rollback purposes */
			char new_mani_path[strlen(new_vers_path) + 11];
			snprintf(new_mani_path, strlen(new_vers_path) + 11, "%s/.Manifest", new_vers_path);
			int fd_new_mani = open(new_mani_path, O_CREAT | O_WRONLY, 0744);
			write(fd_new_mani, write_to_new_mani, strlen(write_to_new_mani));
			close(fd_new_mani);
			close(fd_mani);
			/* Record current .Commit to .History */
			char hist_path[strlen(project) + 30];
			snprintf(hist_path, strlen(project) + 31, ".server_directory/%s/.History", project);
			int fd_hist = open(hist_path, O_WRONLY | O_APPEND);
			write(fd_hist, "push\n", 5);
			char temp[sizeof(version + 1) + 1];
			snprintf(temp, sizeof(version + 1), "%d", version + 1);
			write(fd_hist, temp, strlen(temp));
			write(fd_hist, "\n", 1);
			write(fd_hist, comm_input, strlen(comm_input));
			write(fd_hist, "\n\n", 2);
			close(fd_hist);
			/* Create version-specific .Commit with same contents as client's .Commit ->
			 * necessary for showing rollbacks in .History */
			char new_commit_path[strlen(new_vers_path) + 10];
			snprintf(new_commit_path, strlen(new_vers_path) + 10, "%s/.Commit", new_vers_path);
			int fd_new_commit = open(new_commit_path, O_CREAT | O_WRONLY, 0744);
			write(fd_new_commit, comm_input, strlen(comm_input));
			close(fd_new_commit);
			printf("Push complete!\n");
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);	
			free(recving);
			free(to_send);
		} else if (token[0] == 'u') {
			/* UPDATE */
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
			/* Send client server's .Manifest */
			char *mani_path = (char *) malloc(strlen(token) + 31);
			snprintf(mani_path, strlen(token) + 31, ".server_directory/%s/.Manifest", token);
			char *to_send = (char *) malloc(2);
			int fd_mani = open(mani_path, O_RDONLY);
			int mani_size = get_file_size(fd_mani);
			if (fd_mani < 0 || mani_size < 0) {
				fprintf(stderr, "ERROR: Unable to open .Manifest for project \"%s\".\n", token);
				free(mani_path);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				close(fd_mani);
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
			close(fd_mani);
		} else if (token[0] == 'g') {
			/* UPGRADE */
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
			char *to_send = (char *) malloc(2);
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);
			/* Get .Update from client */
			char *recving = (char *) malloc(sizeof(int) + 1);
			received = recv(client_socket, recving, sizeof(int), 0);
			recving[received] = '\0';
			if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Client could not open local .Update for project \"%s\".\n", token);
				free(recving);
				free(to_send);
				pthread_exit(NULL);
			} else if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Nothing to update for project \"%s\".\n", token);
				free(recving);
				free(to_send);
				pthread_exit(NULL);
			}
			int upd_size = atoi(recving);
			free(recving);
			recving = (char *) malloc(upd_size + 1);
			received = recv(client_socket, recving, upd_size, 0);
			while (received < upd_size) {
				int br = recv(client_socket, recving + received, upd_size, 0);
				received += br;
			}
			recving[received] = '\0';
			char upd_input[received + 1];
			strcpy(upd_input, recving);
			upd_input[received] = '\0';
			/* Open .Manifest */
			char mani_path[strlen(token) + 11];
			snprintf(mani_path, strlen(token) + 11, "%s/.Manifest", token);
			int fd_mani = open(mani_path, O_RDONLY);
			int mani_size = get_file_size(fd_mani);
			if (fd_mani < 0 || mani_size < 0) {
				fprintf(stderr, "ERROR: Cannot open .Manifest for project \"%s\".\n", token);
				free(recving);
				snprintf(to_send, 2, "m");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				close(fd_mani);
				pthread_exit(NULL);
			}
			char mani_input[mani_size + 1];
			int bytes_read = read(fd_mani, mani_input, mani_size);
			close(fd_mani);
			mani_input[bytes_read] = '\0';
			char mani_vers[mani_size + 1];
			strcpy(mani_vers, mani_input);
			char *vers_token = strtok(mani_vers, "\n");
			int version = atoi(vers_token);
			char vers_path[sizeof(version) + strlen(token) + 28];
			snprintf(vers_path, sizeof(version) + strlen(token) + 28, ".server_directory/%s/version%d/", token, version);
			int count = 0;
			char *upd_token;
			/* Basically, reverse of push -> send over contents of files M or A */ 
			int j = 0, k = 0;
			int last_sep = 0;
			int token_len = 0;
			int len = strlen(upd_input);
			int delete_check = 0;
			char *path = NULL;
			for (j = 0; j < len; ++j) {	
				if (upd_input[j] != '\t' && upd_input[j] != '\n') {
					++token_len;
					continue;
				} else {
					upd_token = (char *) malloc(token_len + 1);
					for (k = 0; k < token_len; ++k) {
						upd_token[k] = upd_input[last_sep + k];
					}
					upd_token[token_len] = '\0';
					last_sep += token_len + 1;
					token_len = 0;
					++count;
				}
				if (count % 4 == 1) {
					if (upd_token[0] == 'D') {
						delete_check = 1;
					}
					free(upd_token);
				} else if (count % 4 == 2) {
					free(upd_token);
				} else if (count % 4 == 3) {
					path = (char *) malloc(strlen(upd_token) + 1);
					strcpy(path, upd_token);
					path[strlen(upd_token)] = '\0';
					upd_token += strlen(token) + 1;
					char new_file_path[strlen(upd_token) + strlen(vers_path) + 1];
					snprintf(new_file_path, strlen(upd_token) + strlen(vers_path) + 1, "%s%s", vers_path, upd_token);
					if (delete_check) {
						free(upd_token);
					} else {
						int fd = open(new_file_path, O_RDONLY);
						int size = get_file_size(fd);
						if (fd < 0 || size < 0) {
							fprintf(stderr, "ERROR: Unable to open \"%s\".\n", new_file_path);
							snprintf(to_send, 2, "x");
							sent = send(client_socket, to_send, 2, 0);
							free(to_send);
							free(recving);
							free(upd_token);
							close(fd);
							close(fd_mani);
							pthread_exit(NULL);
						}
						free(to_send);
						to_send = (char *) malloc(sizeof(size) + 1);
						snprintf(to_send, sizeof(size) + 1, "%d", size);
						to_send[sizeof(size)] = '\0';
						sent = send(client_socket, to_send, sizeof(size) + 1, 0);
						free(to_send);
						to_send = (char *) malloc(size + 1);
						int bytes_read = read(fd, to_send, size);
						to_send[bytes_read] = '\0';
						sent = send(client_socket, to_send, size, 0);
						while (sent < size) {
							int bs = send(client_socket, to_send + sent, size, 0);
							sent += bs;
						}
						close(fd);
						free(recving);
						recving = (char *) malloc(2);
						received = recv(client_socket, recving, 1, 0);
						if (recving[0] == 'x') {
							fprintf(stderr, "ERROR: Client failed to upgrade.\n");
							free(to_send);
							free(recving);
							free(upd_token);
							pthread_exit(NULL);
						}
					}
				} else {
					free(upd_token);
				}
			}
			free(to_send);
			to_send = (char *) malloc(sizeof(int));
			snprintf(to_send, sizeof(int), "%d", mani_size);
			free(to_send);
			to_send = (char *) malloc(strlen(mani_input) + 1);
			strcpy(to_send, mani_input);
			to_send[strlen(mani_input)] = '\0';
			sent = send(client_socket, to_send, mani_size, 0);
			while (sent < mani_size) {
				int bs = send(client_socket, to_send + sent, mani_size, 0);
				sent += bs;
			}
			received = recv(client_socket, recving, 1, 0);
			if (recving[0] == 'g') {
				printf("Upgrade successful!\n");
			} else {
				printf("Upgrade failed.\n");
			}
			close(fd_mani);
			free(to_send);
			free(recving);
		} else if (token[0] == 'r') {
			/* ROLLBACK */
			token = strtok(NULL, ":");
			char project[strlen(token) + 1];
			strcpy(project, token);
			project[strlen(token)] = '\0';
			char proj_path[strlen(project) + 22];
			snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
			char *to_send = (char *) malloc(2);
			if (check_dir(proj_path) == -1) {
				snprintf(to_send, 2, "b");
				sent = send(client_socket, to_send, 2, 0);
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", project);
				pthread_exit(NULL);
			}
			char mani_path[strlen(proj_path) + 11];
			snprintf(mani_path, strlen(proj_path) + 11, "%s/.Manifest", proj_path);
			int fd_mani = open(mani_path, O_RDONLY);
			if (fd_mani < 0) {
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				fprintf(stderr, "ERROR: Cannot open .Manifest for project \"%s\".\n", project);
				free(to_send);
				close(fd_mani);
				pthread_exit(NULL);
			}
			/* Get version of client */
			token = strtok(NULL, ":");
			int req_vers = atoi(token);
			char vers[256];
			read(fd_mani, vers, 256);
			char *vers_token = strtok(vers, "\n");
			int serv_vers = atoi(vers_token);
			/* Can only work if requested version is less than server's version */
			if (req_vers >= serv_vers) {
				fprintf(stderr, "ERROR: Invalid version given for rollback request of project \"%s\".\n", project);
				snprintf(to_send, 2, "v");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				close(fd_mani);
				pthread_exit(NULL);
			}
			int rb_check = rollback(proj_path, req_vers);
			if (rb_check == -1) {
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				close(fd_mani);
				pthread_exit(NULL);
			}
			char new_mani_path[strlen(proj_path) + 19 + sizeof(req_vers)];
			snprintf(new_mani_path, strlen(proj_path) + 19 + sizeof(req_vers), "%s/version%d/.Manifest", proj_path, req_vers);
			int fd_new_mani = open(new_mani_path, O_RDONLY);
			int new_size = get_file_size(fd_new_mani);
			if (fd_new_mani < 0 || new_size < 0) {
				fprintf(stderr, "ERROR: Cannot get new input for .Manifest following rollback of project \"%s\".\n", project);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				close(fd_mani);
				close(fd_new_mani);
				pthread_exit(NULL);
			}
			char new_mani_input[new_size + 1];
			int br = read(fd_new_mani, new_mani_input, new_size);
			new_mani_input[br] = '\0';
			fd_mani = open(mani_path, O_TRUNC | O_WRONLY);
			write(fd_mani, new_mani_input, new_size);
			close(fd_new_mani);
			close(fd_mani);
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);
			free(to_send);
			/* Write to .History */
			char hist_path[strlen(proj_path) + 10];
			snprintf(hist_path, strlen(proj_path) + 10, "%s/.History", proj_path);
			int fd_hist = open(hist_path, O_WRONLY | O_APPEND);
			char temp[sizeof(req_vers) + 1];
			snprintf(temp, sizeof(req_vers), "%d", req_vers);
			write(fd_hist, "rollback ", 9);
			write(fd_hist, temp, strlen(temp));
			write(fd_hist, "\n", 1);
			write(fd_hist, temp, strlen(temp));
			write(fd_hist, "\n", 1);
			if (req_vers > 0) {
				char new_commit_path[strlen(proj_path) + 18 + sizeof(req_vers)];
				snprintf(new_commit_path, strlen(proj_path) + 18 + sizeof(req_vers), "%s/version%d/.Commit", proj_path, req_vers);
				int fd_new_comm = open(new_commit_path, O_RDONLY);
				new_size = get_file_size(fd_new_mani);
				char new_comm_input[new_size + 1];
				br = read(fd_new_comm, new_comm_input, new_size);
				new_comm_input[br] = '\0';
				write(fd_hist, new_comm_input, strlen(new_comm_input));
				write(fd_hist, "\n\n", 2);
				close(fd_new_comm);
			}
			close(fd_hist);

			/* Success */
			printf("Rollback successful!\n");
		} else if (token[0] == 'h') {
			token = strtok(NULL, ":");
			char project[strlen(token) + 1];
			strcpy(project, token);
			project[strlen(token)] = '\0';
			char *proj_path = (char *) malloc(strlen(token) + 22);
			snprintf(proj_path, strlen(token) + 22, ".server_directory/%s", token);
			char *to_send = (char *) malloc(2);
			if (check_dir(proj_path) == -1) {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", project);
				snprintf(to_send, 2, "b");
				sent = send(client_socket, to_send, 2, 0);
				pthread_exit(NULL);
			}
			char hist_path[strlen(proj_path) + 10];
			snprintf(hist_path, strlen(proj_path) + 10, "%s/.History", proj_path);
			int fd_hist = open(hist_path, O_RDONLY);
			int hist_size = get_file_size(fd_hist);
			if (hist_size < 0 || fd_hist < 0)  {
				fprintf(stderr, "ERROR: Cannot open .History for project \"%s\".\n", project);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);  
				pthread_exit(NULL);
			}
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 1, 0);
			free(to_send);
			int sending_size = sizeof(hist_size);

			to_send = (char *) malloc(sending_size + 1);
			snprintf(to_send, sending_size, "%d", hist_size);
			to_send[sending_size] = '\0';
			sent = send(client_socket, to_send, sending_size, 0);

			free(to_send);
			sending_size = hist_size;
			to_send = (char *) malloc(sending_size + 1);
			int br = read(fd_hist, to_send, sending_size);
			to_send[br] = '\0';
			sent = send(client_socket, to_send, sending_size, 0);
			while (sent < sending_size) {
				int bs = send(client_socket, to_send + sent, sending_size, 0);
				sent += bs;
			}

			close(fd_hist);
			printf("Sent history of project \"%s\" to client!\n", project);
		} else if (token[0] == 'x') {
			fprintf(stderr, "ERROR: Mishap on client's end.\n");
		}
		pthread_mutex_unlock(&cntx->lock);
	} if (!keep_running) {
		printf("Server closed.\n");
	}
	pthread_exit(NULL);
}

