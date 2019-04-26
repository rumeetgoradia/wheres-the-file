#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "clientfunctions.h"

unsigned int tokenize(char *path, char *input, int *version, char *hash) {
	if (input == NULL) {
		return 0;	
	}
	char whole_string[strlen(input)];
	strcpy(whole_string, input);
	char *token;
	unsigned int byte_count = 0;
	unsigned int prev_bytes = 0;
	unsigned int check_bytes = 0;
	short check = 0;
	token = strtok(whole_string, "\n\t");
	prev_bytes = strlen(token) + 1;
	byte_count += prev_bytes;
	if (version != NULL) {
		*version = 0;
	}
	while (token != NULL) {
		if (version != NULL) {
			*version = atoi(token);
		}
		token = strtok(NULL, "\n\t");
		if (token == NULL) {
			break;
		}
		if (check == 1) {
			if (strcmp(token, hash) == 0) {
				return -1;
			} else {
				return byte_count - prev_bytes - check_bytes;
			}
		}
		if (strcmp(token, path) == 0) {
			check = 1;
		}
		check_bytes = prev_bytes;
		prev_bytes = strlen(token) + 1;
		byte_count += prev_bytes;
	}
	if (version != NULL) {
		*version = -1;
	}
	return strlen(input);
	
}

int add(int fd_manifest, char *hashcode, char *path, char *input) {
	int *version = (int *) malloc(sizeof(int));
	*version = 0;
	int move = tokenize(path, input, version, hashcode);
	if (move == 0) {
		fprintf(stderr, "ERROR: Could not read \".Manifest\" file.\n");
		return -1;
	}
	if (move == -1) {
		fprintf(stderr, "ERROR: File already up-to-date in local \".Manifest\" file.\n");
		return -1;
	}
	char buff[move];
	read(fd_manifest, buff, move);
	int update = *version + 1;
	char *new_version = (char *) malloc(sizeof(update) / (sizeof(char) + 1));
	sprintf(new_version, "%d", update);
	write(fd_manifest, new_version, strlen(new_version));
	write(fd_manifest, "\t", 1);
	write(fd_manifest, path, strlen(path));
	write(fd_manifest, "\t", 1);
	write(fd_manifest, hashcode, strlen(hashcode));
	write(fd_manifest, "\n", 1);
	close(fd_manifest);
	return 0;
}

int remover(int fd_manifest, char *path, char *input) {
	int *version = (int *) malloc(sizeof(int));
	*version = 0;
	char dashes[64];
	memset(dashes, '-', sizeof(dashes));
	int move = tokenize(path, input, version, dashes);
	if (move == strlen(input)) {
		fprintf(stderr, "ERROR: File \"%s\" not in \".Manifest\" file.\n", path);
		return -1;
	}
	if (move == -1) {
		fprintf(stderr, "ERROR: File \"%s\" already removed from \".Manifest\" file.\n", path);
		return -1;
	}
	int total_char = 2 + strlen(path);
	if (*version == 0) {
		++total_char;
	} else {
		total_char += (int) log(*version);
	}
	char buff[move + total_char];
	read(fd_manifest, buff, move + total_char);
	write(fd_manifest, dashes, strlen(dashes));
	return 0;
}
