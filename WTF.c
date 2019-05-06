#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <limits.h>
#include "helperfunctions.h"

int main (int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "ERROR: Not enough arguments.\n");
		return EXIT_FAILURE;
	}
	if (strcmp("configure", argv[1]) == 0) {
		if (argc < 4) {
			fprintf(stderr, "ERROR: Need IP address and port number.\n");
			return EXIT_FAILURE;
		} else if (argc > 4) {
			fprintf(stderr, "ERROR: Too many arguments.\n");
			return EXIT_FAILURE;
		}
		int conf_file = open("./.configure", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (conf_file < 0) {
			fprintf(stderr, "ERROR: Could not open or create .configure file.\n");
			return EXIT_FAILURE;
		}
		write(conf_file, argv[2], strlen(argv[2]));
		write(conf_file, "\n", 1);
		write(conf_file, argv[3], strlen(argv[3]));
		write(conf_file, "\n", 1);
		close(conf_file);
	} else if (strcmp("add", argv[1]) == 0 || strcmp("remove", argv[1]) == 0) {
		if (argc < 4) {
			fprintf(stderr, "ERROR: Need project name and file name.\n");
			return EXIT_FAILURE;
		} else if (argc > 4) {
			fprintf(stderr, "ERROR: Too many arguments.\n");
			return EXIT_FAILURE;
		}
		/* Ensure first argument is a directory */
		struct stat pstat;
		if (stat(argv[2], &pstat) != 0) {
			fprintf(stderr, "ERROR: Project \"%s\" does not exist.\n", argv[2]);
			return EXIT_FAILURE;
		}
		int is_file = S_ISREG(pstat.st_mode);
		if (is_file != 0) {
			fprintf(stderr, "ERROR: First argument must be a directory.\n");
			return EXIT_FAILURE;
		}
		/* Ensure file exists in project */
		char *path = (char *) malloc(strlen(argv[2]) + strlen(argv[3]) + 2);
		snprintf(path, strlen(argv[2]) + strlen(argv[3]) + 2, "%s/%s", argv[2], argv[3]);
		int fd_file = open(path, O_RDONLY);
		if (fd_file < 0 && strcmp("add", argv[1]) == 0) {
			fprintf(stderr, "ERROR: File \"%s\" does not exist in project \"%s\".\n", argv[3], argv[2]);
			free(path);
			close(fd_file);
			return EXIT_FAILURE;
		}
		/* Setup .Manifest file path */
		char *path_mani = (char *) malloc(strlen(argv[2]) + 11);
		snprintf(path_mani, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
		int fd_manifest = open(path_mani, O_RDWR | O_CREAT, 0744);
		if (fd_manifest < 0) {
			fprintf(stderr, "ERROR: Could not open or create .Manifest.\n");
			free(path);
			free(path_mani);
			close(fd_manifest);
			return EXIT_FAILURE;
		}
		char *temp2 = (char *) malloc(sizeof(char) * INT_MAX);
		int total_length = read(fd_manifest, temp2, INT_MAX);
		char *mani_input = NULL;
		if (total_length != 0) {
			mani_input = (char *) malloc(sizeof(char) * (total_length + 1));
			strcpy(mani_input, temp2);
		} else {
			mani_input = (char *) malloc(sizeof(char) * 3);
			mani_input = "0\n";
			write(fd_manifest, "0\n", 2);
		}
		free(temp2);
		if (strcmp(argv[1], "add") == 0) {
			int size = get_file_size(fd_file);
			char input[size + 1];
			read(fd_file, input, size);
			unsigned char hash[SHA256_DIGEST_LENGTH];
			SHA256(input, strlen(input), hash);
			char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}	
			if (add(fd_manifest, hashed, path, mani_input, 0) == -1) {
				free(path);
				free(path_mani);
				close(fd_manifest);
				return EXIT_FAILURE;
			}
			free(path);
			free(path_mani);
			close(fd_manifest);
		} else {	
			if (remover(fd_manifest, path, mani_input) == -1) {
				free(path);
				free(path_mani);
				close(fd_manifest);
				return EXIT_FAILURE;
			}
			free(path);
			free(path_mani);	
			close(fd_manifest);
		}
	} else {
		int client_socket;
		struct addrinfo hints, *res, *ptr;
		char *token;
		int fd_conf = open("./.configure", O_RDONLY);
		if (fd_conf < 0) {
			fprintf(stderr, "ERROR: Could not open \".configure\" file.\n");
			return EXIT_FAILURE;
		}
		char conf_buff[50];
		read(fd_conf, conf_buff, 50);
		token = strtok(conf_buff, "\n");
		char *host = (char *) malloc(strlen(token) + 1);
		strcpy(host, token);
		token = strtok(NULL, "\n");
		char *port = (char *) malloc(strlen(token) + 1);
		strcpy(port, token);
		int received, sent;	
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(host, port, &hints, &res) != 0) {
			fprintf(stderr, "ERROR: getaddrinfo() failed.\n");
			return EXIT_FAILURE;
		}
		ptr = res;
		printf("Waiting for server...\n");
		while(1) {
			if (ptr == NULL) {
				ptr = res;
			}
			if ((client_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) < 0) {
				fprintf(stderr, "ERROR: Could not open client-side socket.\n");
				ptr = ptr->ai_next;
				sleep(3);
				continue;
			}
			if (connect(client_socket, ptr->ai_addr, ptr->ai_addrlen) < 0) {
				close(client_socket);	
				ptr = ptr->ai_next;
				sleep(3);
				continue;
			}
			break;
		}
		freeaddrinfo(res);
		if (strcmp(argv[1], "create") == 0) {
			if (argc < 3) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name.\n");
				return EXIT_FAILURE;
			}
			if (argc > 3) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name.\n");
				return EXIT_FAILURE;
			}
			char sending[strlen(argv[2]) + 3];
			char recv_buff[15];
			snprintf(sending, strlen(argv[2]) + 3, "c:%s", argv[2]);	
			sent = send(client_socket, sending, strlen(sending), 0);
			received = recv(client_socket, recv_buff, sizeof(recv_buff) - 1, 0);
			recv_buff[received] = '\0';
			if (recv_buff[0] == 'x') {
				fprintf(stderr, "ERROR: Project \"%s\" already exists on server.\n", argv[2]);
				return EXIT_FAILURE;
			} else {
				struct stat st = {0};
				
				if (stat(argv[2], &st) == -1) {
					mkdir(argv[2], 0744);
					char *new_mani_path = (char *) malloc(strlen(argv[2]) + 13);
					snprintf(new_mani_path, strlen(argv[2]) + 13, "./%s/.Manifest", argv[2]);
					int fd_mani = open(new_mani_path, O_CREAT | O_WRONLY, 0744);
					write(fd_mani, "0\n", 2);
					close(fd_mani);
					free(new_mani_path);
					printf("Project \"%s\" initialized successfully!\n", argv[2]);
				} else {
					fprintf(stderr, "ERROR: Project \"%s\" created on server, but directory of same name already exists on client-side.\n", argv[2]);
					return EXIT_FAILURE;
				}
			}
		} else if (strcmp(argv[1], "destroy") == 0) {
			if (argc < 3) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name.\n");
				return EXIT_FAILURE;
			}
			if (argc > 3) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name.\n");
				return EXIT_FAILURE;
			}
			char sending[strlen(argv[2]) + 3];
			snprintf(sending, strlen(argv[2]) + 3, "d:%s", argv[2]);
			sent = send(client_socket, sending, strlen(sending), 0);
			char recv_buff[2];
			received = recv(client_socket, recv_buff, sizeof(recv_buff) - 1, 0);
			recv_buff[received] = '\0';
			if (recv_buff[0] == 'g') {
				printf("Project \"%s\" deleted on server successfully!\n", argv[2]);
			} else if (recv_buff[0] == 'b') {
				fprintf(stderr, "ERROR: Failed to delete project \"%s\" on server.\n", argv[2]);
				return EXIT_FAILURE;
			} else {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				return EXIT_FAILURE;
			}
		} else if (strcmp(argv[1], "currentversion") == 0) {
			if (argc < 3) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name.\n");
				return EXIT_FAILURE;
			}
			if (argc > 3) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name.\n");
				return EXIT_FAILURE;
			}
			char sending[strlen(argv[2]) + 3];
			snprintf(sending, strlen(argv[2]) + 3, "v:%s", argv[2]);
			sent = send(client_socket, sending, strlen(sending), 0);
			char recv_buff[256];
			received = recv(client_socket, recv_buff, 255, 0);
			if (recv_buff[0] == 'x') {
				fprintf(stderr, "ERROR: Failed to get current version for project \"%s\" from server.\n", argv[2]);
				return EXIT_FAILURE;
			}
			recv_buff[received] = '\0';
			int file_size = atoi(recv_buff);
			char *version = (char *) malloc(file_size + 2);
			received = recv(client_socket, version, file_size, 0);
			if (version[0] == 'x') {
				fprintf(stderr, "ERROR: Failed to get current version for project \"%s\" from server.\n", argv[2]);
				return EXIT_FAILURE;
			} else if (version[0] == 'b') {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				return EXIT_FAILURE;
			}
			if (iscntrl(version[0])) {
				++version;
			}
			if (received < file_size) {
				int offset = 0;
				int remaining = file_size - received;
				while ((received = recv(client_socket, (version + offset), remaining, 0)) > 0 && offset < file_size && remaining > 0) {
					printf("receiving: %s", version + offset);
					remaining -= received;
					offset += received;
				}
			}
			version[file_size] = '\0';
			token = strtok(version, "\n");
			printf("PROJECT: %s (Version %s)\n", argv[2], token);
			printf("----------------------------\n");
			int count = 1;
			while (token != NULL) {
				token = strtok(NULL, "\n\t");
				if (token == NULL && count == 1) {
					printf("No entries\n");
					break;
				} else if (token == NULL) {
					break;
				}
				if (count % 3 == 0) {
					++count;
					continue;
				}
				if (count % 3 == 2) {
					printf("%s\n", token);
				} else if (count % 3 == 1) {
					printf("%s\t", token);
				}
				++count;
			}
		} else if (strcmp(argv[1], "commit") == 0) {
			if (argc < 3) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name.\n");
				return EXIT_FAILURE;
			}
			if (argc > 3) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name.\n");
				return EXIT_FAILURE;
			}
			int sending_size = strlen(argv[2]) + 3;
			char *to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "o:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);

			char *recving = (char *) malloc(sizeof(int));
			received = recv(client_socket, recving, sizeof(int), 0);	

			if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Failed to get server's .Manifest for project \"%s\" from server.\n", argv[2]);
				free(to_send);
				free(recving);
				return EXIT_FAILURE;
			} else if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				free(to_send);
				free(recving);
				return EXIT_FAILURE;
			}
			int server_mani_size = atoi(recving);
			free(recving);

			recving = (char *) malloc(server_mani_size + 1);

			received = recv(client_socket, recving, server_mani_size, 0);
/*			while (received < server_mani_size) {
				int bytes_recv = recv(client_socket, recving + bytes_recv, server_mani_size,0);
				received += bytes_recv;
			} */

			char server_mani_input[received + 1];
			strcpy(server_mani_input, recving);
			server_mani_input[received] = '\0';

/*			if (iscntrl(server_mani_input[0])) {
				server_mani_input = &(server_mani_input[1]);
			} */
			char *client_mani = (char *) malloc(strlen(argv[2]) + 11);
			snprintf(client_mani, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
			int fd_mani = open(client_mani, O_RDWR);
			int client_mani_size = get_file_size(fd_mani);

			if (fd_mani < 0 || client_mani_size < 0) {
				fprintf(stderr, "ERROR: Unable to open local .Manifest for project \"%s\".\n", argv[2]);
				free(to_send);
				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				free(recving);
				free(client_mani);
				return EXIT_FAILURE;
			}
			char client_mani_input[client_mani_size];
			read(fd_mani, client_mani_input, client_mani_size); 
			char get_version[256];
			strcpy(get_version, client_mani_input);
			char *vers_token = strtok(get_version, "\n");
			int client_mani_vers = atoi(vers_token);			
			vers_token = strtok(server_mani_input, "\n");

			if (client_mani_vers != atoi(vers_token)) {
				fprintf(stderr, "ERROR: Local \"%s\" project has not been updated.\n", argv[2]);
				free(to_send);
				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				free(recving);
				free(client_mani);
				return EXIT_FAILURE;
			}

			/* Setup .Commit file path */
			char *path_comm = (char *) malloc(strlen(argv[2]) + 10);
			snprintf(path_comm, strlen(argv[2]) + 10, "%s/.Commit", argv[2]);	
			int fd_comm = open(path_comm, O_RDWR | O_CREAT | O_TRUNC, 0744);	
			if (fd_comm < 0) {
				printf("ERROR: Could not open or create .Commit for project \"%s\".\n", argv[2]);
				free(to_send);
                                to_send = (char *) malloc(2);
                                snprintf(to_send, 2, "x");
                                sent = send(client_socket, to_send, 2, 0);
                                free(to_send);
                                free(recving);
                                free(client_mani);
				free(path_comm);
				close(fd_comm);
				return EXIT_FAILURE;
			}

			if (commit(fd_comm, client_mani_input, server_mani_input, fd_mani) == -1) {
				fprintf(stderr, "ERROR: Local \"%s\" project is not up-to-date with server.\n", argv[2]);
				free(to_send);
                                to_send = (char *) malloc(2);
                                snprintf(to_send, 2, "b");
                                sent = send(client_socket, to_send, 2, 0);
                                free(to_send);
                                free(recving);
                                free(client_mani);
				return EXIT_FAILURE;
			}

			free(to_send);
			int comm_size = get_file_size(fd_comm);
			if (comm_size < 0) {
                                printf("ERROR: Could not get size of .Commit for \"%s\" project.\n", argv[2]);
                                free(to_send);
                                to_send = (char *) malloc(2);
                                snprintf(to_send, 2, "x");
                                sent = send(client_socket, to_send, 2, 0);
                                free(to_send);
                                free(recving);
                                free(client_mani);
                                free(path_comm);
                                close(fd_comm);
                                return EXIT_FAILURE;
                        }
			sending_size = sizeof(comm_size);
			to_send = (char *) malloc(sending_size + 1);
			snprintf(to_send, sending_size, "%d", comm_size);
			sent = send(client_socket, to_send, sending_size, 0);
			lseek(fd_comm, 0, SEEK_SET);
			free(to_send);
			to_send = (char *) malloc(comm_size);
			int bytes_read = read(fd_comm, to_send, comm_size); 
			sent = send(client_socket, to_send, comm_size, 0);
			received = recv(client_socket, recving, 2, 0);

			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Server failed to create its own \".Commit\" file for \"%s\" project.\n", argv[2]);
				free(recving);
				free(to_send);
				return EXIT_FAILURE;
			} else if (recving[0] == 'g') {
				free(recving);
				free(to_send);
				printf("Commit successful!\n");
			}
			close(fd_comm);	
		} else if (strcmp(argv[1], "push") == 0) {
			if (argc < 3) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name.\n");
				return EXIT_FAILURE;
			}
			if (argc > 3) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name.\n");
				return EXIT_FAILURE;
			}
			int sending_size = 3 + strlen(argv[2]);
			char *to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "p:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);
			while (sent < sending_size) {
				int bytes_sent  = send(client_socket, to_send + sent, sending_size, 0);
				sent += bytes_sent;
			}
			char *recving = (char *) malloc(2);
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				return EXIT_FAILURE;
			}
			char comm_path[strlen(argv[2]) + 9];
			snprintf(comm_path, strlen(argv[2]) + 9, "%s/.Commit", argv[2]);
			int fd_comm = open(comm_path, O_RDONLY);
			if (fd_comm < 0) {
				fprintf(stderr, "ERROR: Failed to open local .Commit for \"%s\" project.\n", argv[2]);
				return EXIT_FAILURE;
			}
			int size = get_file_size(fd_comm);
			if (size == -1) {
				fprintf(stderr, "ERROR: Failed to get size of local .Commit for \"%s\" project.\n", argv[2]);
				return EXIT_FAILURE;
			}
			/* Reading .Commit's input */
			char comm_input[size + 1];
			int bytes_read = read(fd_comm, comm_input, size);
			comm_input[size] = '\0';
			free(to_send);
			sending_size = sizeof(bytes_read);
			to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "%d", bytes_read);
			sent = send(client_socket, to_send, sending_size, 0);
			sending_size = bytes_read;
			free(to_send);
			to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "%s", comm_input);
			sent = send(client_socket, to_send, sending_size, 0);
			received = recv(client_socket, recving, 1, 0);
			if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Server failed during .Commit lookup for project \"%s\".\n", argv[2]);
				remove(comm_path);
				return EXIT_FAILURE;
			} else if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Matching .Commit could not be found on server's copy of \"%s\".\n", argv[2]);
				remove(comm_path);
				return EXIT_FAILURE;
			}
			received = recv(client_socket, recving, 1, 0);
			if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Server could not open its .Manifest for project \"%s\".\n", argv[2]);
			} else if (recving[0] == 'g') {
				printf("Server initializing new version of project \"%s\".\n", argv[2]);
			}
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Server could not instantiate new version of project \"%s\".\n", argv[2]);
				return EXIT_FAILURE;
			}
			/* Open .Manifest */
			char mani_path[strlen(argv[2]) + 11];
			snprintf(mani_path, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
			int fd_mani = open(mani_path, O_RDONLY);
			int mani_size = get_file_size(fd_mani);
			if (fd_mani < 0 || mani_size < 0) {
				free(to_send);
				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
			}
			char mani_buff[mani_size + 1];
			bytes_read = read(fd_mani, mani_buff, mani_size);
			char mani_jic[mani_size + 1];
			char *new_mani_buff = (char *) malloc(mani_size + 1);
			strncpy(mani_jic, mani_buff, bytes_read);
			strncpy(new_mani_buff, mani_buff, bytes_read);
			char *mani_token = strtok(mani_buff, "\n");
			int mani_vers = atoi(mani_token);
			new_mani_buff += strlen(mani_token) + 1;
			char write_to_new_mani[strlen(new_mani_buff) + 2 + sizeof(mani_vers + 1)];
			snprintf(write_to_new_mani, strlen(new_mani_buff) + 2 + sizeof(mani_vers + 1), "%d\n%s", mani_vers + 1, new_mani_buff);
			fd_mani = open(mani_path, O_RDWR | O_TRUNC);
			write(fd_mani, write_to_new_mani, strlen(write_to_new_mani));
			int count = 0;
			char *comm_token;
			int j = 0, k = 0;
			int last_sep = 0;
			int token_len = 0;
			int len = strlen(comm_input);
			int delete_check = 0;
			char *path = NULL;
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
					}
					free(comm_token);
				} else if (count % 4 == 2) {
					free(comm_token);
				} else if (count % 4 == 3) {
					if (!delete_check) {
						free(to_send);
						path = (char *) malloc(strlen(comm_token) + 1);
						strncpy(path, comm_token, strlen(comm_token));
						int fd_file = open(comm_token, O_RDONLY);
						int file_size = get_file_size(fd_file);	
						if (fd_file < 0 || file_size < 0) {
							to_send = (char *) malloc(2);
							snprintf(to_send, 2, "x");
							sent = send(client_socket, to_send, 2, 0);
							fd_mani = open(mani_path, O_WRONLY | O_TRUNC);
							write(fd_mani, mani_jic, mani_size);
							close(fd_mani);
							remove(comm_path);
							close(fd_file);
							free(path);
							free(comm_token);
							return EXIT_FAILURE;
						}
						sending_size = sizeof(file_size);
						to_send = (char *) malloc(sending_size);
						snprintf(to_send, sending_size, "%d", file_size);
						sent = send(client_socket, to_send, sending_size, 0);
						free(to_send);
						sending_size = file_size;
						to_send = (char *) malloc(sending_size);
						read(fd_file, to_send, sending_size);
						sent = send(client_socket, to_send, sending_size, 0);
						while (sent < file_size) {
							int bytes_sent = send(client_socket, to_send + sent, sending_size, 0);
							sent += bytes_sent;
						}
						received = recv(client_socket, recving, 2, 0);
						if (recving[0] == 'x') {
							fprintf(stderr, "ERROR: Server could not open new copy of \"%s\" in project \"%s\".\n", comm_token, argv[2]);
							fd_mani = open(mani_path, O_WRONLY | O_TRUNC);
							write(fd_mani, mani_jic, mani_size);
							close(fd_mani);
							free(path);
							remove(comm_path);
							close(fd_file);
							free(comm_token);
							return EXIT_FAILURE;
						}
						close(fd_file);
						free(comm_token);
					}
				} else if (count % 4 == 0) {	
					char hashed[strlen(comm_token) + 1];
					strcpy(hashed, comm_token);
					hashed[strlen(comm_token)] = '\0';
					if (!delete_check) {
						add(fd_mani, hashed, path, write_to_new_mani, 1);
					} else {
						delete_check = 0;
					}
					free(comm_token);
					free(path);
				}
			}
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'g') {
				printf("Push succeeded!\n");
				remove(comm_path);
			} else {
				printf("Push failed.\n");
				remove(comm_path);
			}
		}
		close(client_socket);
	}
	return EXIT_SUCCESS;
}
