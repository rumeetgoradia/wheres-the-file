#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <limits.h>
#include "clientfunctions.h"

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
	}
	if (strcmp("add", argv[1]) == 0 || strcmp("remove", argv[1]) == 0) {
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
		strcpy(path, argv[2]);
		if (argv[2][strlen(argv[2]) - 1] != '/') {
			path[strlen(argv[2])] = '/';
		}
		strcat(path, argv[3]); 
		int fd_file = open(path, O_RDWR, 0644);
		if (fd_file < 0) {
			printf("ERROR: File \"%s\" does not exist in project \"%s\".\n", argv[3], argv[2]);
			close(fd_file);
			return EXIT_FAILURE;
		}
		/* Setup .Manifest file path */
		char *path_mani = (char *) malloc(strlen(argv[2]) + strlen(".Manifest") + 2);
		strcpy(path_mani, argv[2]);
		if (argv[2][strlen(argv[2]) - 1] != '/') {
			path_mani[strlen(argv[2])] = '/';
		}	
		strcat(path_mani, ".Manifest");
		int fd_manifest = open(path_mani, O_RDONLY | O_CREAT, 0644);
		if (fd_manifest < 0) {
			printf("ERROR: Could not open or create .Manifest file.\n");
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
		char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
		int i = 0;
		for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
			sprintf(hashed + (i * 2), "%02x", hash[i]);
		}
		char *temp2 = (char *) malloc(sizeof(char) * INT_MAX);
		total_length = read(fd_manifest, temp2, INT_MAX);
		close(fd_manifest);
		char *mani_input = NULL;
		if (total_length != 0) {
			mani_input = (char *) malloc(sizeof(char) * (total_length + 1));
			strcpy(mani_input, temp2);
		} else {
			mani_input = (char *) malloc(sizeof(char) * 3);
			mani_input = "0\n";
		}
		free(temp2);
		if (strcmp(argv[1], "add") == 0) {
			if (add(path_mani, hashed, path, mani_input) == -1) {
				return EXIT_FAILURE;
			}
		} else {
			
			if (remover(path_mani, path, mani_input) == -1) {
				return EXIT_FAILURE;
			}
		}
	}
	return EXIT_SUCCESS;
}
