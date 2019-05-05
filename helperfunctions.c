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
			if (strcmp(token, hash) == 0) {
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
	if (move == -1) {
		fprintf(stderr, "ERROR: File already up-to-date in .Manifest.\n");
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
	char dashes[65] = "----------------------------------------------------------------";
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
	char *token = strtok(other_mani, "\n");
	int count = 0;
	int check = 0;
	int version = 0;
	while (token != NULL) {
		token = strtok(NULL, "\n\t");
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
	return 0;
}

int commit(int fd_comm, char *client_mani, char *server_mani, int fd_mani) {

	char temp[strlen(client_mani) + 1];
	strcpy(temp, client_mani);
	char *token = strtok(temp, "\n");
	int count = 0;
	int version = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
	char *path = NULL;
	int bytes = 2;
	while (token != NULL) {
		token = strtok(NULL, "\t\n");
		++count;
		if (token == NULL) {
			if (bytes < strlen(client_mani) - 1) {
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
				continue;
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
