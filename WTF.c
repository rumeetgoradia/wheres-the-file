/**********************************
 * Aditi Singh and Rumeet Goradia 
 * as2811, rug5
 * Section 04
 * CS214
 **********************************/

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
	/* Base argument check */
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
		/* Store IP address and port number in a file */
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
		printf("Configuration successful.\n");
	} else if (strcmp("add", argv[1]) == 0 || strcmp("remove", argv[1]) == 0) {
		/* Command-specific argument check at the beginning of every command */
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
		if (strstr(argv[3], argv[2]) == NULL || (strstr(argv[3], argv[2]) != argv[3] && argv[3][0] != '.' && argv[3][1] != '/')) {
			if (argv[2][strlen(argv[2]) - 1] != '/') {
				snprintf(path, strlen(argv[2]) + strlen(argv[3]) + 2, "%s/%s", argv[2], argv[3]);
			} else {
				snprintf(path, strlen(argv[2]) + strlen(argv[3]) + 2, "%s%s", argv[2], argv[3]);
			}	
		} else {
			free(path);
			path = (char *) malloc(strlen(argv[3]) + 1);
			strcpy(path, argv[3]);
			path[strlen(argv[3])] = '\0';
		}
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
		/* If local project existed before "create" was called, chances are it doesn't have initialized .Manifest yet */
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
			/* Get input of file and hash it */
			int size = get_file_size(fd_file);
			char input[size + 1];
			read(fd_file, input, size);
			input[size] = '\0';
			unsigned char hash[SHA256_DIGEST_LENGTH];
			SHA256(input, strlen(input), hash);
			char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}	
			/* Add to .Manifest */
			if (add(fd_manifest, hashed, path, mani_input, 0) == -1) {
				free(path);
				free(path_mani);
				close(fd_manifest);
				return EXIT_FAILURE;
			}
			printf("Addition of \"%s\" successful.\n", path);
			free(path);
			free(path_mani);
			close(fd_manifest);
		} else {
			/* No need for hash or to check if file exists, just get rid of it from .Manifest */
			if (remover(fd_manifest, path, mani_input) == -1) {
				free(path);
				free(path_mani);
				close(fd_manifest);
				return EXIT_FAILURE;
			}
			printf("Removal of \"%s\" successful.\n", path);
			free(path);
			free(path_mani);	
			close(fd_manifest);
		}
	} else {
		/* Set up socket */
		int client_socket;
		struct addrinfo hints, *res, *ptr;
		char *token;
		int fd_conf = open("./.configure", O_RDONLY);
		/* Get info from .Configure, if it exists */
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
			/* CREATE */
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
			/* For each different command, send server single char (representing command) with project name */
			snprintf(sending, strlen(argv[2]) + 3, "c:%s", argv[2]);	
			sent = send(client_socket, sending, strlen(sending), 0);
			received = recv(client_socket, recv_buff, sizeof(recv_buff) - 1, 0);
			recv_buff[received] = '\0';
			/* Check for specific "error codes" sent from server */
			if (recv_buff[0] == 'x') {
				fprintf(stderr, "ERROR: Project \"%s\" already exists on server.\n", argv[2]);
				return EXIT_FAILURE;
			} else {
				/* If directory exists, initialize, else issue error */
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
			/* DESTROY */
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
			/* CURRENTVERSION */
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
			/* For all files sent from server, get file size first, then get actual file contents */
			/* Getting .Manifest from server */
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
			/* Fix to random error that kept popping up */
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
			/* Print file number and file path, but skip hash */
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
			/* COMMIT */
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
			/* Check if .Update exists; if yes, check if it's empty */
			char upd_path[strlen(argv[2]) + 9];
			snprintf(upd_path, strlen(argv[2]) + 9, "%s/.Update", argv[2]);
			int fd_upd = open(upd_path, O_RDONLY);
			if (fd_upd >= 0) {
				char temp[2];
				int br = read(fd_upd, temp, 1);
				if (br > 0) {
					snprintf(to_send, sending_size, "x");
					sent = send(client_socket, to_send, 2, 0);
					fprintf(stderr, "ERROR: Non-empty .Update exists locally for project \"%s\".\n", argv[2]);
					free(to_send);
					close(fd_upd);
					return EXIT_FAILURE;
				}
				close(fd_upd);
			}
			snprintf(to_send, sending_size, "o:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);
			
			/* Get server's .Manifest */
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
			while (received < server_mani_size) {
				int bytes_recv = recv(client_socket, recving + received, server_mani_size,0);
				received += bytes_recv;
			} 
			char server_mani_input[received + 1];
			strcpy(server_mani_input, recving);
			server_mani_input[received] = '\0';
			/* Get client's .Manifest */
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
			char client_mani_input[client_mani_size + 1];
			int bread = read(fd_mani, client_mani_input, client_mani_size); 
			client_mani_input[bread] = '\0';

			/* Check versions; if they don't match, cease operation */
			char get_version[256];
			strcpy(get_version, client_mani_input);

			char *vers_token = strtok(get_version, "\n");

			int client_mani_vers = atoi(vers_token);
			char serv_temp[strlen(server_mani_input)];

			snprintf(serv_temp, strlen(server_mani_input), "%s", server_mani_input);
//			strcpy(serv_temp, server_mani_input);

			char *server_vers_token = strtok(serv_temp, "\n");

			if (client_mani_vers != atoi(server_vers_token)) {
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

			/* Setup .Commit */
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
			
			/* Run helper function to fill .Commit */

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
                                fprintf(stderr, "ERROR: Could not get size of .Commit for \"%s\" project.\n", argv[2]);
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
			/* If .Commit is empty, don't bother doing anything else */
			if (comm_size == 0) {
				fprintf(stderr, "ERROR: .Commit for project \"%s\" is empty.\n", argv[2]);
				free(to_send);
				free(recving);
				free(client_mani);
				free(path_comm);
				close(fd_comm);
				return EXIT_FAILURE;
			}
			lseek(fd_comm, 0, SEEK_SET);
			free(to_send);
			to_send = (char *) malloc(comm_size);
			int bytes_read = read(fd_comm, to_send, comm_size); 
			sent = send(client_socket, to_send, comm_size, 0);
			received = recv(client_socket, recving, 2, 0);
			/* Ensure server was able to create its own .Commit */
			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Server failed to create its own .Commit for project \"%s\".\n", argv[2]);
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
			/* PUSH */
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
			/* Check if .Update exists, is not empty, and contains an M code; if yes, fail*/
			char upd_path[strlen(argv[2]) + 9];
                        snprintf(upd_path, strlen(argv[2]) + 9, "%s/.Update", argv[2]);
                        int fd_upd = open(upd_path, O_RDONLY);
                        if (fd_upd >= 0) {
				char temp[2];
                                int br = read(fd_upd, temp, 1);
                                if (br > 0) {
					lseek(fd_upd, 0, 0);
					int size = get_file_size(fd_upd);
					char upd_input[size + 1];
					br = read(fd_upd, upd_input, size);
					upd_input[br] = '\0';
					char *upd_token = strtok(upd_input, "\t\n");
					while (upd_token != NULL) {
						if (strcmp(upd_token, "M") == 0) {
							snprintf(to_send, sending_size, "x");
							sent = send(client_socket, to_send, 2, 0);
							fprintf(stderr, "ERROR: Non-empty .Update exists locally for project \"%s\".\n", argv[2]);
							free(to_send);
							close(fd_upd);
	                	                        return EXIT_FAILURE;
						}
						upd_token = strtok(NULL, "\t\n");
					}
                                }
                                close(fd_upd);
                        }
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
			/* Send .Commit to server */
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
			if (bytes_read == 0) {
				fprintf(stderr, "ERROR: Empty .Commit for project \"%s\".\n", argv[2]);
/*				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "d"); */
				return EXIT_FAILURE;
			}
			snprintf(to_send, sending_size, "%d", bytes_read);
			sent = send(client_socket, to_send, sending_size, 0);
			sending_size = bytes_read;
			free(to_send);
			to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "%s", comm_input);
			sent = send(client_socket, to_send, sending_size, 0);
			received = recv(client_socket, recving, 1, 0);
			/* Check whether server could find matching .Commit */
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
			/* Same process as server: create copy of current .Manifest and version with updated version number
			* Implement new .Manifest immediately, but if failure, return old .Manifest */
			char mani_jic[mani_size + 1];
			char *new_mani_buff = (char *) malloc(mani_size + 1);
			strncpy(mani_jic, mani_buff, bytes_read);
			strncpy(new_mani_buff, mani_buff, bytes_read);
			char *mani_token = strtok(mani_buff, "\n");
			int mani_vers = atoi(mani_token);
			new_mani_buff += strlen(mani_token) + 1;
			char *write_to_new_mani = malloc(strlen(new_mani_buff) + 2 + sizeof(mani_vers + 1));
			snprintf(write_to_new_mani, strlen(new_mani_buff) + 2 + sizeof(mani_vers + 1), "%d\n%s", mani_vers + 1, new_mani_buff);
			close(fd_mani);
			fd_mani = open(mani_path, O_RDWR | O_TRUNC);
			write(fd_mani, write_to_new_mani, strlen(write_to_new_mani));
			/* Tokenize .Commit and make changes to local .Manifest */
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
							close(fd_mani);
							fd_mani = open(mani_path, O_WRONLY | O_TRUNC);
							/* Failure: Revert to old .Manifest */
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
							close(fd_mani);
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
					/* For loop finishes before looking at last hash */
					char hashed[strlen(comm_token) + 1];
					strcpy(hashed, comm_token);
					hashed[strlen(comm_token)] = '\0';
					if (!delete_check) {
						add(fd_mani, hashed, path, write_to_new_mani, 1);
						int new_mani_size = get_file_size(fd_mani);
						free(write_to_new_mani);
						write_to_new_mani = (char *) malloc(new_mani_size + 1);
						lseek(fd_mani, 0, 0);
						int br = read(fd_mani, write_to_new_mani, new_mani_size);
						write_to_new_mani[br] = '\0';	
					} else {
						delete_check = 0;
					}
					free(comm_token);
					free(path);
				}
			}
			if (token_len > 0) {
				comm_token = (char *) malloc(token_len + 1);
				for (j = 0; j < token_len; ++j) {
					comm_token[j] = comm_input[last_sep + j];
				}
				comm_token[token_len] = '\0';
				if (!delete_check) {
					add(fd_mani, comm_token, path, write_to_new_mani, 1);
        	                }
                        	free(comm_token);
//	                        free(path);
                	}
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'g') {
				printf("Push succeeded!\n");
				remove(comm_path);
			} else {
				printf("Push failed.\n");
				remove(comm_path);
			}
		} else if (strcmp(argv[1], "update") == 0) {
			/* UPDATE */
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
			snprintf(to_send, sending_size, "u:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);
			char *recving = (char *) malloc(sizeof(int));
			received = recv(client_socket, recving, sizeof(int), 0);	
			/* Just need server's .Manifest to complete */
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
			
			char server_mani_input[received + 1];
			strcpy(server_mani_input, recving);
			server_mani_input[received] = '\0';
		
			/* Get version of server's .Manifest */
			char serv_temp[strlen(server_mani_input)];
			snprintf(serv_temp, strlen(server_mani_input), "%s", server_mani_input);
			char *vers_token = strtok(serv_temp, "\n");
			int server_version = atoi(vers_token);
		
			/* Open local .Manifest */
			char *client_mani = (char *) malloc(strlen(argv[2]) + 11);
			snprintf(client_mani, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
			int fd_mani = open(client_mani, O_RDWR);
			int client_mani_size = get_file_size(fd_mani);
			if (fd_mani < 0 || client_mani_size < 0) {
				fprintf(stderr, "ERROR: Unable to open local .Manifest for project \"%s\".\n", argv[2]);
				free(to_send);
				free(recving);
				return EXIT_FAILURE;
			}
			char client_mani_input[client_mani_size];
			int bytes_read = read(fd_mani, client_mani_input, client_mani_size);
			client_mani_input[bytes_read] = '\0';

			/* Get version of client's .Manifest */
			char client_temp[bytes_read + 1];
			strcpy(client_temp, client_mani_input);
			client_temp[bytes_read] = '\0';
			vers_token = strtok(client_temp, "\n");
			int client_version = atoi(vers_token);
			lseek(fd_mani, 0, 0);
		
			/* Set up .Update */
			char *path_upd = (char * ) malloc(strlen(argv[2]) + 9);
			snprintf(path_upd, strlen(argv[2]) + 9, "%s/.Update", argv[2]);
			int fd_upd = open(path_upd, O_RDWR | O_CREAT | O_TRUNC, 0744);
			if (fd_upd < 0) {
				fprintf(stderr, "ERROR: Could not open or create .Update for project \"%s\".\n", argv[2]);
				free(to_send);
                                free(recving);
				free(path_upd);
				close(fd_upd);
				return EXIT_FAILURE;
			}
			/* Use helper function to check if any conflicts and to put in update data */
		
			int upd_check = update(fd_upd, client_mani_input, server_mani_input, client_version, server_version);
			
			if (upd_check == -1) {
				free(to_send);
				remove(path_upd);
				close(fd_upd);
			} else if (upd_check == 0) {
				fprintf(stderr, "ERROR: Take care of all conflicts for project \"%s\" before attempting to update.\n", argv[2]);
				free(to_send);
				free(recving);
				remove(path_upd);
				close(fd_upd);
			} else {
				if (get_file_size(fd_upd) > 0) {
					printf(".Update created successfully for project \"%s\"!\n", argv[2]);
				} else {
					printf("Already up to date!\n");
				}
				free(recving);
				free(to_send);
				close(fd_upd);
			}
		} else if (strcmp(argv[1], "upgrade") == 0) {
			/* UPGRADE */
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
			snprintf(to_send, sending_size, "g:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);
			char *recving = (char *) malloc(2);
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				return EXIT_FAILURE;
			}
			/* Open local .Update */
			char *path_upd = (char * ) malloc(strlen(argv[2]) + 9);
			snprintf(path_upd, strlen(argv[2]) + 9, "%s/.Update", argv[2]);
			int fd_upd = open(path_upd, O_RDWR);
			int size_upd = get_file_size(fd_upd);
			if (fd_upd < 0 || size_upd < 0) {
				fprintf(stderr, "ERROR: Could not open or create .Update for project \"%s\". Please perform an update first.\n", argv[2]);
				free(to_send);
				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
                                free(recving);
				free(path_upd);
				close(fd_upd);
				return EXIT_FAILURE;
			}
			if (size_upd == 0) {
				printf(".Update is empty, project \"%s\" up-to-date.\n");
				free(to_send);
				to_send = (char *) malloc(2);
				snprintf(to_send, 2, "b");
				sent = send(client_socket, to_send, 2, 0);
				free(recving);
				free(path_upd);
				free(to_send);
				close(fd_upd);
			}
			free(to_send);
			/* Send .Update to server */
			sending_size = sizeof(size_upd);
			to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "%d", size_upd);
			sent = send(client_socket, to_send, sending_size, 0);
			free(to_send);
			sending_size = size_upd;
			to_send = (char *) malloc(sending_size + 1);
			int br = read(fd_upd, to_send, sending_size);
			to_send[br] = '\0';
			sent = send(client_socket, to_send, sending_size, 0);
			while (sent < br) {
				int bs = send(client_socket, to_send + sent, sending_size, 0);
				sent += bs;
			}
			char upd_input[br + 1];
			strcpy(upd_input, to_send);
			upd_input[br] = '\0';
			/* Tokenize .Update data */
			int count = 0;
			char *upd_token;
			int j = 0, k = 0;
			int last_sep = 0;
			int token_len = 0;
			int len = strlen(upd_input);
			int delete_check = 0, modify_check = 0;
			char *file_path = NULL;
			printf("about to enter for loop\n");
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
				printf("%d\n", count);
				if (count % 4 == 1) {
					if (upd_token[0] == 'D') {
						delete_check = 1;
					} else if (upd_token[0] == 'M') {
						modify_check = 1;
					}
					free(upd_token);
				} else if (count % 4 == 2) {
					free(upd_token);	
				} else if (count % 4 == 3) {
					if (delete_check == 1) {
						remove(upd_token);
					} else {
						free(recving);
						recving = (char *) malloc(sizeof(int) + 1);
						received = recv(client_socket, recving, sizeof(int), 0);
						recving[received] = '\0';
						printf("recved %d bytes: %s\n", received, recving);
						/* For anything that was M code or A code, get file content's from server */
						if (recving[0] == 'x') {
							fprintf(stderr, "ERROR: Server could not send contents of \"%s\".\n", upd_token);
							free(file_path);
							free(upd_token);
							close(fd_upd);
							return EXIT_FAILURE;
						}
						printf("got past check\n");
						int file_size = atoi(recving);
						printf("size: %d\n", file_size);
						free(recving);
						recving = (char *) malloc(file_size + 1);
						received = recv(client_socket, recving, file_size, 0);
						/* while (received < file_size) {
							int brecv = recv(client_socket, recving + received, file_size, 0);
							received += brecv;
						} */
						recving[received] = '\0';
						printf("recved %d bytes: %s\n", received, recving);
						int fd_file = open(upd_token, O_WRONLY | O_TRUNC);
						if (!modify_check) {
							close(fd_file);
							fd_file = open(upd_token, O_WRONLY | O_CREAT, 0744);
						}
						if (fd_file < 0) {
							free(recving);
							snprintf(to_send, 2, "x");
							sent = send(client_socket, to_send, 2, 0);
							free(to_send);
							fprintf(stderr, "ERROR: Failed to open \"%s\" file in project \"%s\".\n", upd_token, argv[2]);
							free(upd_token);
							close(fd_file);
							close(fd_upd);
						}
						write(fd_file, recving, strlen(recving));
						snprintf(to_send, 2, "g");
						sent = send(client_socket, to_send, 2, 0); 
						free(upd_token);
					}
				} else {
					free(upd_token);
				}
			}
			printf("Got this done\n");
			close(fd_upd);
			remove(path_upd);
			free(recving);
			recving = (char *) malloc(sizeof(int) + 1);
			received = recv(client_socket, recving, sizeof(int), 0);
			recving[received] = '\0';
			/* New .Manifest will be the same as server's, so just get server's .Manifest */
			if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Server unable to send .Manifest for project \"%s\".\n", argv[2]);
				free(recving);
				free(to_send);
				return EXIT_FAILURE;
			}
			int mani_size = atoi(recving);
			free(recving);
			recving = (char *) malloc(mani_size + 1);
			received = recv(client_socket, recving, mani_size, 0);
			while (received < mani_size) {
				int brecv = recv(client_socket, recving + received, mani_size, 0);
				received += brecv;
			}
			/* Open local .Manifest */
			char path_mani[strlen(argv[2]) + 11];
			snprintf(path_mani, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
			int fd_mani = open(path_mani, O_WRONLY | O_TRUNC);
			if (fd_mani < 0) {
				free(recving);
				snprintf(to_send, 2, "x");
				sent = send(client_socket, to_send, 2, 0);
				free(to_send);
				fprintf(stderr, "ERROR: Unable to open .Manifest for project \"%s\".\n", argv[2]);
				close(fd_mani);
			}
			write(fd_mani, recving, mani_size);
			close(fd_mani);
			snprintf(to_send, 2, "g");
			sent = send(client_socket, to_send, 2, 0);
			free(recving);
			free(to_send);
			printf("Upgrade successful!\n");
		} else if (strcmp(argv[1], "rollback") == 0) {
			/* ROLLBACK */
			if (argc < 4) {
				fprintf(stderr, "ERROR: Not enough arguments. Please input the project name and desired version number.\n");
				return EXIT_FAILURE;
			}
			if (argc > 4) {
				fprintf(stderr, "ERROR: Too many arguments. Please input only the project name and desired version number.\n");
				return EXIT_FAILURE;
			}
			/* Send rollback code, project name, and version all in 1 go */
			int sending_size = strlen(argv[2]) + strlen(argv[3]) + 4;
			char *to_send = (char *) malloc(sending_size);
			snprintf(to_send, sending_size, "r:%s:%s", argv[2], argv[3]);
			sent = send(client_socket, to_send, sending_size, 0);
			char *recving = (char *) malloc(2);
			received = recv(client_socket, recving, 2, 0);
			if (recving[0] == 'b') {
				fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
			} else if (recving[0] == 'x') {
				fprintf(stderr, "ERROR: Something went wrong on server during rollback of project \"%s\".\n", argv[2]);
			} else if (recving[0] == 'v') {
				fprintf(stderr, "ERROR: Invalid version number inputted. The version number must be less than the current version of project \"%s\" on the server.\n", argv[2]);
			} else if (recving[0] == 'g') {
				printf("Rollback successful!\n");
			}
		} else if (strcmp(argv[1], "history") == 0) {
			/* HISTORY */
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
			snprintf(to_send, sending_size, "h:%s", argv[2]);
			sent = send(client_socket, to_send, sending_size, 0);
			char *recving = (char *) malloc(2);
			received = recv(client_socket, recving, 1, 0);
			if (recving[0] == 'b' || recving[0] == 'x') {
				if (recving[0] == 'b') {
					fprintf(stderr, "ERROR: Project \"%s\" does not exist on server.\n", argv[2]);
				} else {
					fprintf(stderr, "ERROR: Server could not send history of project \"%s\".\n", argv[2]);
				}
				free(to_send);
				free(recving);
				return EXIT_FAILURE;
			}
			/* Get .History file from server */
			free(recving);
			recving = (char *) malloc(sizeof(int) + 1);
			received = recv(client_socket, recving, sizeof(int), 0);	
			recving[received] = '\0';
			int size = atoi(recving);
			free(recving);
			recving = (char *) malloc(size + 1);
			received = recv(client_socket, recving, size, 0);
			while (received < size) {
				int br = recv(client_socket, recving + received, size, 0);
				received += br;
			}
			recving[received] = '\0';
			/* Print history */
			printf("%s", recving);
			free(recving);
			free(to_send);
		} else {
			/* If argv[1] didn't match any of the commands, send error code to server */ 
			char to_send[2] = "x";
			sent = send(client_socket, to_send, 2, 0);
		}
		close(client_socket);
	}
	return EXIT_SUCCESS;
}
