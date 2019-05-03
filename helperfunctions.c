#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

unsigned int tokenize(char *path, char *input, char *hash) {
	if (input == NULL) {
		return 0;	
	}
	char whole_string[strlen(input)];
	strcpy(whole_string, input);
	char *token;
	unsigned int byte_count = 0;
	unsigned int prev_bytes = 0;
	short check = 0;
	token = strtok(whole_string, "\n\t");
	prev_bytes = strlen(token) + 1;
	byte_count += prev_bytes;
	while (token != NULL) {
		token = strtok(NULL, "\n\t");
		if (token == NULL) {
			break;
		}
		if (check == 1) {
			if (strcmp(token, hash) == 0) {
				return -1;
			} else {
				return byte_count;
			}
		}
		if (strcmp(token, path) == 0) {
			check = 1;
		}
		prev_bytes = strlen(token) + 1;
		byte_count += prev_bytes;
	}
	return strlen(input);
	
}

int add(int fd_manifest, char *hashcode, char *path, char *input) {
	int *version = (int *) malloc(sizeof(int));
	int move = tokenize(path, input, hashcode);
	if (move == 0) {
		fprintf(stderr, "ERROR: Could not read \".Manifest\" file.\n");
		return -1;
	}
	if (move == -1) {
		fprintf(stderr, "ERROR: File already up-to-date in local \".Manifest\" file.\n");
		return -1;
	}
/*	char buff[move];
	read(fd_manifest, buff, move); */
	lseek(fd_manifest, move, SEEK_SET);
	if (move == strlen(input)) {
		write(fd_manifest, "0\t", 2);
		write(fd_manifest, path, strlen(path));
		write(fd_manifest, "\t", 1);
		write(fd_manifest, hashcode, strlen(hashcode));	
		write(fd_manifest, "\n", 1);
	} else {
		write(fd_manifest, hashcode, strlen(hashcode));
	}
	return 0;
}

int remover(int fd_manifest, char *path, char *input) {
	char dashes[65] = "----------------------------------------------------------------";
	int move = tokenize(path, input, dashes);
	if (move == strlen(input)) {
		fprintf(stderr, "ERROR: File \"%s\" not in \".Manifest\" file.\n", path);
		return -1;
	}
	if (move == -1) {
		fprintf(stderr, "ERROR: File \"%s\" already removed from \".Manifest\" file.\n", path);
		return -1;
	}
/*	char buff[move];
	read(fd_manifest, buff, move); */
	lseek(fd_manifest, move, SEEK_SET);
	write(fd_manifest, dashes, strlen(dashes));
	return 0;
}

int check_dir(char *path) {
	DIR *dr = opendir(path);
	if (dr == NULL) {
		return -1;	
	}
	return 0;
}

int remove_dir(char *path) {
	DIR *dr;
	size_t path_len = strlen(path);
	int ret = -1;
	if (!(dr = opendir(path))) {
		fprintf(stderr, "ERROR: Could not open project \"%s\" on server's side.\n", (path + 17));
		return ret;
	}
	struct dirent *de;
	ret = 0;
	while ((de = readdir(dr)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}
		char *new_path = (char *) malloc(strlen(de->d_name) + strlen(path) + 2);
		if (new_path != NULL) {
			snprintf(new_path, strlen(de->d_name) + strlen(path) + 2, "%s/%s", path, de->d_name);
			if (de->d_type == DT_DIR) {
				ret = remove_dir(new_path);
			} else {
				ret = remove(new_path);
			}
			if (ret < 0) {
				fprintf(stderr, "ERROR: Failed to delete \"%s\" from server.", new_path);
				return ret;
			}
		}
		free(new_path);
	}
	ret = rmdir(path);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Failed to delete \"%s\" from server.", path);
	}
	return ret;
}
