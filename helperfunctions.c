#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <openssl/sha.h>

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
	if (move == -2) {
		return -2;
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
	if (move == -2) {
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

int get_file_size(int fd) {
	struct stat st = {0};
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "ERROR: fstat() failed.\n");
		return -1;
	}
	return st.st_size;
}

int commit_check(char *path, char *hash, int given_version, char *other_mani, int dashes) {
	char *token = strtok(other_mani, "\t\n");
	int count = 0;
	int check = 0;
	int version = 0;
	while (token != NULL) {
		token = strtok(NULL, "\t\n");
		if (token == NULL) {
			break;
		}
		++count;
		if (check == 1) {
			if (strcmp(token, hash) != 0 && version >= given_version) {
				return -1;
			} else if ((strcmp(token, hash) != 0 && !dashes) || (strcmp(token, "----------------------------------------------------------------") != 0 && dashes)) {
				return 2;
			}
			break;
		}
		if (count % 3 == 1) {
			version = atoi(token);
		} else if (count % 3 == 2) {
			if (strcmp(token, path) == 0) {
				check = 1;
			}
		}
			
	}
	return dashes;
}

int commit(int fd_comm, char *client_mani, char *server_mani, int fd_mani) {
	char *token = strtok(client_mani, "\t\n");
	int count = 0;
	int version = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
	char *path = NULL;
	while (token != NULL) {
		token = strtok(NULL, "\t\n");
		if (token == NULL) {
			break;
		}
		++count;
		if (count % 3 == 1) {
			version = atoi(token);
			++version;
		} else if (count % 3 == 2) {
			int fd = open(token, O_RDONLY);
			int size = get_file_size(fd);
			char buff[size + 1];
			read(fd, buff, size);
			SHA256(buff, strlen(buff), hash);
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}
			path = malloc(strlen(token) + 1);
			snprintf(path, strlen(token) + 1, "%s", token);
		} else {
//			int check = add(0, hashed, path, client_mani, 0);
			int check = strcmp(token, hashed);
			int dash_check = strcmp(token, "----------------------------------------------------------------");
			int comm_check = commit_check(path, hash, version, server_mani, dash_check);
			if (dash_check != 0) {
				if (comm_check == 2) {
					write(fd_comm, "U\t", 2);
				} else if (comm_check == 0) {
					write(fd_comm, "A\t", 2);
				}
			}
			if ((check != 0 || dash_check == 0) && comm_check == -1) {
				return -1;
			}
			if ((check == 0 && comm_check == -1) || (dash_check == 0 && comm_check == 0)) {
				return 0;
			}
			if (dash_check == 0 && comm_check == 2) {
				write(fd_comm, "D\t", 2);
			}
			char *version_string = (char *) malloc(sizeof(version) + 1);
			snprintf(version_string, sizeof(version) + 1, "%d", version);
			write(fd_comm, version_string, strlen(version_string));
			write(fd_comm, "\t", 1);
			write(fd_comm, path, strlen(path));
			write(fd_comm, "\t", 1);
			write(fd_comm, hashed, strlen(hashed));
		}
	}	
		
}
