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
		printf("ERROR: Not enough arguments.\n");
		return EXIT_FAILURE;
	}
	if (strcmp("configure", argv[1]) == 0) {
		if (argc < 4) {
			printf("ERROR: Need IP address and port number.\n");
			return EXIT_FAILURE;
		} else if (argc > 4) {
			printf("ERROR: Too many arguments.\n");
			return EXIT_FAILURE;
		}
		int conf_file = open("./.configure", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (conf_file < 0) {
			printf("ERROR: Could not open or create .configure file.\n");
			return EXIT_FAILURE;
		}
		write(conf_file, argv[2], strlen(argv[2]));
		write(conf_file, "\n", 1);
		write(conf_file, argv[3], strlen(argv[3]));
		write(conf_file, "\n", 1);
		close(conf_file);
	} else if (strcmp("add", argv[1]) == 0 || strcmp("remove", argv[1]) == 0) {
		if (argc < 4) {
			printf("ERROR: Need project name and file name.\n");
			return EXIT_FAILURE;
		} else if (argc > 4) {
			printf("Error: Too many arguments.\n");
			return EXIT_FAILURE;
		}
		/* Ensure first argument is a directory */
		struct stat pstat;
		if (stat(argv[2], &pstat) != 0) {
			printf("ERROR: Project \"%s\" does not exist.\n", argv[2]);
			return EXIT_FAILURE;
		}
		int is_file = S_ISREG(pstat.st_mode);
		if (is_file != 0) {
			printf("ERROR: First argument must be a directory.\n");
			return EXIT_FAILURE;
		}
		/* Ensure file exists in project */
		char *path = (char *) malloc(strlen(argv[2]) + strlen(argv[3]) + 2);
		snprintf(path, strlen(argv[2]) + strlen(argv[3]) + 2, "%s/%s", argv[2], argv[3]);
		int fd_file = open(path, O_RDWR, 0644);
		if (fd_file < 0) {
			printf("ERROR: File \"%s\" does not exist in project \"%s\".\n", argv[3], argv[2]);
			free(path);
			close(fd_file);
			return EXIT_FAILURE;
		}
		/* Setup .Manifest file path */
		char *path_mani = (char *) malloc(strlen(argv[2]) + 11);
		snprintf(path_mani, strlen(argv[2]) + 11, "%s/.Manifest", argv[2]);
		int fd_manifest = open(path_mani, O_RDWR | O_CREAT, 0644);
		if (fd_manifest < 0) {
			printf("ERROR: Could not open or create .Manifest file.\n");
			free(path);
			free(path_mani);
			close(fd_manifest);
			return EXIT_FAILURE;
		}
		char *temp = (char *) malloc(sizeof(char) * INT_MAX);
		int total_length = read(fd_file, temp, INT_MAX);
		char *input = (char *) malloc(sizeof(char) * (total_length + 1));
		strcpy(input, temp);
		free(temp);
		unsigned char hash[SHA256_DIGEST_LENGTH];
		SHA256(input, strlen(input), hash);
		free(input);
		char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
		int i = 0;
		for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
			sprintf(hashed + (i * 2), "%02x", hash[i]);
		}
		char *temp2 = (char *) malloc(sizeof(char) * INT_MAX);
		total_length = read(fd_manifest, temp2, INT_MAX);
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
			if (add(fd_manifest, hashed, path, mani_input) == -1) {
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
					int fd_mani = open(new_mani_path, O_CREAT | O_WRONLY, 0644);
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
			} else if (version[0] = 'b') {
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
			printf("------------------------\n");
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
		}
/*		while(1) {
			if (received == 0) {
				fprintf(stderr, "ERROR: Server closed the connection.\n");
				close(client_socket);
				return EXIT_FAILURE;
			} else if (received < 0) {
				fprintf(stderr, "ERROR: Client-side socket recv() failed.\n");
				close(client_socket);
				return EXIT_FAILURE;
			} else {
				recv_buff[received] = '\0';
				printf("received: %s\n", recv_buff);
				received = recv(client_socket, recv_buff, sizeof(recv_buff) - 1, 0);
			}
		} */
		close(client_socket);
	}
	return EXIT_SUCCESS;
}
