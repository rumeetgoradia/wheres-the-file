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

char dashes[65] = "----------------------------------------------------------------";

unsigned int tokenize(char *path, char *input, char *hash, int flag, int *version) {
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
	int count = 0;

	while (token != NULL) {
		token = strtok(NULL, "\n\t");
		++count;
		if (token == NULL) {
			break;
		}
		if (count % 3 == 1 && version != NULL) {
			*version = atoi(token);
		}
		if (check == 1) {
			if (strcmp(token, hash) == 0 && !flag) {
					return -1;
			} else {
				return byte_count - (flag * check_bytes) - (flag * prev_bytes);
			}
		}
		if (strcmp(token, path) == 0) {
			check = 1;
		}
		check_bytes = prev_bytes;
		prev_bytes = strlen(token) + 1;
		byte_count += prev_bytes;
	}
	return strlen(input);
}

int add(int fd_manifest, char *hashcode, char *path, char *input, int flag) {
	int *vers = (int *) malloc(sizeof(int));
	*vers = 0;
	int move = tokenize(path, input, hashcode, flag, vers);
	if (move == 0) {
		fprintf(stderr, "ERROR: Could not read .Manifest.\n");
		return -1;
	}
	if (move == -1 && !flag) {
		fprintf(stderr, "ERROR: File already up-to-date in .Manifest.\n");
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
	} else if (!flag) {
		write(fd_manifest, hashcode, strlen(hashcode));
	} else {
		int update = *vers + 1;
		char update_string[sizeof(update) + 2];
		snprintf(update_string, sizeof(update) + 2, "%d\t", update);
		write(fd_manifest, update_string, strlen(update_string));
		write(fd_manifest, path, strlen(path));
		write(fd_manifest, "\t", 1);
		write(fd_manifest, hashcode, strlen(hashcode));
		write(fd_manifest, "\n", 1);
	}
	return 0;
}

int remover(int fd_manifest, char *path, char *input) {
	int move = tokenize(path, input, dashes, 0, NULL);
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
				fprintf(stderr, "ERROR: Failed to delete \"%s\" from server. It is probably currently in use.\n", new_path);
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

int commit_check(int version, char *path, char *hash, char *other_mani) {
	char mani_input[strlen(other_mani)];
	strcpy(mani_input, other_mani);
	char *token = strtok(mani_input, "\t\n");
	int count = 0;
	int hash_check = 0;
	int vers = 0;
	while (token != NULL) {
		token = strtok(NULL, "\t\n");
		++count;
		if (token == NULL) {
			break;
		}
		if (count % 3 == 1) {
			vers = atoi(token);
		} else if (count % 3 == 2) {
			if (strcmp(path, token) == 0) {
				hash_check = 1;
			}
		} else if (count % 3 == 0) {
			if (hash_check) {
				int check = strcmp(hash, token);
				if (check != 0 && vers >= version) {
					return -1;
				} else if (check != 0) {
					return 1;
				} else if (check == 0) {
					return 0;
				}
			}
		}
	}
	return 2;
} 

int commit(int fd_comm, char *client_mani, char *server_mani) {	
	int len = strlen(client_mani);
	char temp[len + 1];
	strcpy(temp, client_mani);
	char *token = strtok(temp, "\n");
	int count = 0;
	int version = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
	char *path = NULL;
	int bytes = 2;
	int delete_check = 0;
	while (token != NULL) {
		token = strtok(NULL, "\t\n");
		++count;
		if (token == NULL) {
			if (bytes < len - 1) {	
				strcpy(temp, client_mani);
				token = strtok(&(temp[bytes - 1]), "\t\n"); 	
			} else {
				break;
			}
		}	
		if (count % 3 == 1) {
			version = atoi(token);
			++version;
		} else if (count % 3 == 2) {
			int fd = open(token, O_RDONLY);
			int size = get_file_size(fd);
			path = malloc(strlen(token) + 1);
			snprintf(path, strlen(token) + 1, "%s", token);
			path[strlen(token)] = '\0';
			if (fd < 0 || size < 0) {
				delete_check = 1;	
				bytes += strlen(token) + 1;
				continue;	
			}
			char buff[size + 1];
			read(fd, buff, size);
			buff[size] = '\0';
			SHA256(buff, strlen(buff), hash);
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}
		} else {
			int token_equals_dashes = strcmp(token, dashes);
			int comm_check;
			if (token_equals_dashes != 0 && delete_check) {
				free(path);
				return -1;
			} else if (token_equals_dashes == 0) {
				delete_check = 0;
				strcpy(hashed, dashes);
				comm_check = commit_check(version - 1, path, dashes, server_mani);
			} else {
				comm_check = commit_check(version - 1, path, token, server_mani);
			}
			int token_equals_hash = strcmp(token, hashed);
			if (comm_check == -1) {
				return -1;
			}	
			if (token_equals_dashes == 0 && comm_check == 1) {
				write(fd_comm, "D\t", 2);
			} else if (token_equals_dashes != 0 && comm_check == 2) {
				write(fd_comm, "A\t", 2);
			} else if (token_equals_hash != 0 && comm_check != 2) {
				write(fd_comm, "M\t", 2);
			} else {
				bytes += strlen(token) + 1;
				continue;
			}
			char *version_string = (char *) malloc(sizeof(version) + 1);
			snprintf(version_string, sizeof(version) + 1, "%d", version);
			write(fd_comm, version_string, strlen(version_string));
			write(fd_comm, "\t", 1);
			write(fd_comm, path, strlen(path));
			write(fd_comm, "\t", 1);
			write(fd_comm, hashed, strlen(hashed));
			write(fd_comm, "\n", 1);
			free(version_string);
		}
		bytes += strlen(token) + 1;
	}
	return 1;	
}

int delete_commits(char *proj_path, char *name) {
	DIR *dir;
	if (!(dir = opendir(proj_path))) {
		fprintf(stderr, "ERROR: Could not open \"%s\" on server.\n", proj_path);
		return -1;
	}
	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		if (strstr(de->d_name, ".Commit") != NULL && strcmp(de->d_name, name) != 0) {
			char *comm_path = (char *) malloc(strlen(proj_path) + strlen(de->d_name) + 1);
			snprintf(comm_path, strlen(proj_path) + strlen(de->d_name) + 1, "%s%s", proj_path, de->d_name);
			if (de->d_type != DT_DIR) {
				remove(comm_path);
			}
			free(comm_path);
		}
	}
	return 0;
}

int push_check(char *project, char *comm_input) {
	char path[20 + strlen(project)];
	snprintf(path, strlen(project) + 20, ".server_directory/%s/", project);
	DIR *dir;
	if (!(dir = opendir(path))) {
		fprintf(stderr, "ERROR: Could not open project \"%s\" on server.\n", project);
		return -1;
	}
	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		if (strstr(de->d_name, ".Commit") != NULL) {
			char *comm_path = (char *) malloc(strlen(path) + strlen(de->d_name) + 1);
			snprintf(comm_path, strlen(path) + strlen(de->d_name) + 1, "%s%s", path, de->d_name);
			int fd_comm = open(comm_path, O_RDONLY);
			if (fd_comm < 0) {
				free(comm_path);
				continue;
			}
			int size = get_file_size(fd_comm);
			if (size <= 0) {
				free(comm_path);
				continue;
			}
			char input[size + 1];
			int bytes_read = read(fd_comm, input, size);
			input[size] = '\0';
			input[size - 1] = '\0';
			if (strcmp(input, comm_input) == 0) {
				free(comm_path);
				return delete_commits(path, de->d_name);
			}
			free(comm_path);
		}
	}
	return 1;
}

int dir_copy(char *src, char *dest) {
	DIR *dir;
	struct dirent *de;
/*	char temp_dest[strlen(dest) + 2];
	char temp_src[strlen(src) + 2]; */
	if ((dir = opendir(src)) == NULL) {
		fprintf(stderr, "ERROR: Cannot open directory \"%s\".\n", src);
		return -1;
	}
	while ((de = readdir(dir)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}
		char *new_src_path = (char *) malloc(strlen(src) + strlen(de->d_name) + 2);
		char *new_dest_path = (char *) malloc(strlen(dest) + strlen(de->d_name) + 2);
		snprintf(new_src_path, strlen(src) + strlen(de->d_name) + 2, "%s/%s", src, de->d_name);
		snprintf(new_dest_path, strlen(dest) + strlen(de->d_name) + 2, "%s/%s", dest, de->d_name);
		if (de->d_type == DT_DIR) {
			mkdir(new_dest_path, 0744);
			if (dir_copy(new_src_path, new_dest_path) != 0) {
				return -1;
			} else {
				return 0;
			}
		} else {
			int fd_src_file = open(new_src_path, O_RDONLY);
			if (fd_src_file < 0) {
				fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", new_src_path);
				return -1;
			}
			int fd_dest_file = open(new_dest_path, O_CREAT | O_WRONLY, 0744);
			if (fd_dest_file < 0) {
				fprintf(stderr, "ERROR: Cannot create file \"%s\".\n", new_dest_path);
				return -1;
			}
			int size = get_file_size(fd_src_file);
			if (size < 0) {
				fprintf(stderr, "ERROR: Cannot get size of file \"%s\".\n", new_src_path);
				return -1;
			}
			char input[size + 1];
			read(fd_src_file, input, size);
			write(fd_dest_file, input, size);
		}
		free(new_src_path);
		free(new_dest_path);
	}
	return 0;
}

int update_check(char *mani, int client_version, int server_version, char *hash, int file_version, char *file_check) {
	int count = -1;
	char *mani_token;
	int j = 0, k = 0;
	int last_sep = 0;
	int token_len = 0;
	int len = strlen(mani);
	int version = 0;
	char *file_path = NULL;
	int check = 0;
	for (j = 0; j < len; ++j) {
		if (mani[j] != '\t' && mani[j] != '\n') {
			++token_len;
			continue;
		} else {
			mani_token = (char *) malloc(token_len + 1);
			for (k = 0; k < token_len; ++k) {
					mani_token[k] = mani[last_sep + k];
				}
			comm_token[token_len] = '\0';
			last_sep += token_len + 1;
			token_len = 0;
			++count;
		}
		if (count == 0) {
			free(mani_token);
			continue;
		}
		if (count % 3 == 1) {
			version = atoi(mani_token);
			free(mani_token);
		} else if (count % 3 == 2) {
			if (strcmp(file_check, mani_token) == 0) {
				check = 1;
			}
			free(mani_token);
		} else if (count % 3 == 0) {
			if (check == 1) {
				if (strcmp(hash, token) == 0 && file_version == version && client_version == server_version) {
					free(mani_token);
					return 2;
				} else if (strcmp(hash, token) != 0 && client_version == server_version) {
					free(mani_token);
					return 1;
				} else if (strcmp(hash, token) != 0 && client_version != server_version && file_version != version) {
					free(mani_token);
					return -1;
				}
				free(mani_token);
				break;
			}
		}
	}
	if (client_version == server_version) {
		return 1;
	} else {
		return 4;
	}
}

int update(int fd_upd, char *client_mani, char *server_mani, int client_version, int server_version) {
	int count = -1;
	char *mani_token;
	int j = 0, k = 0;
	int last_sep = 0;
	int token_len = 0;
	int len = strlen(client_mani);
	int version = 0;
	char *file_path = NULL;
	int print = 1;
	for (j = 0; j < len; ++j) {
		if (client_mani[j] != '\t' && client_input[j] != '\n') {
				++token_len;
				continue;
		} else {
			mani_token = (char *) malloc(token_len + 1);
			for (k = 0; k < token_len; ++k) {
				mani_token[k] = client_mani[last_sep + k];
			}
			comm_token[token_len] = '\0';
			last_sep += token_len + 1;
			token_len = 0;
			++count;
		}
		if (count == 0) {
			free(mani_token);
			continue;
		}
		if (count % 3 == 1) {
			version = atoi(mani_token);
		} else if (count % 3 == 2) {
			int fd = open(token, O_RDONLY);
			int size = get_file_size(fd);
			if (fd < 0 || size < 0) {
				fprintf(stderr, "Cannot read \"%s\".", token);
				return -1;
			}
			file_path = (char *) malloc(strlen(token) + 1);
			strcpy(file_path, token);
			file_path[strlen(token) + 1];
			char buffer[size + 1];
			read(fd, buffer, size);
			buffer[size] = '\0';
			unsigned char hash[SHA256_DIGEST_LENGTH];
			SHA256(buffer, strlen(buffer), hash);
			int i = 0;
			char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}
			hashed[SHA256_DIGEST_LENGTH * 2] = '\0';
			int upd_check = update_check(server_mani, client_version, server_version, hashed, version, token);
			if (upd_check == -1) {
				print = 0;
				printf("CONFLICT: %s\n", mani_token);
			}
			if (print) {
				if (upd_check == 2) {
					write(fd_upd, "M\t", 2);
					printf("M\t");
				} else if (upd_check == 4) {
					write(fd_upd, "D\t", 2);
					printf("D\t");
				}
				char vers[sizeof(version) + 1];
				snprintf(vers, sizeof(version), "%d", version);
				vers[sizeof(version)] = '\0';
				write(fd_upd, vers, strlen(vers));
				write(fd_upd, "\t", 1);
				write(fd_upd, mani_token, strlen(mani_token));
				write(fd_upd, "\t", 1);
				write(fd_upd, hashed, strlen(hashed));
			}
			close(fd);
			free(mani_token);
		} else {
			free(mani_token);
		}
	}
	
}
