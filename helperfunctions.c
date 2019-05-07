/**********************************
 * Aditi Singh and Rumeet Goradia 
 * as2811, rug5
 * Section 04
 * CS214
 **********************************/

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

/* Global string of hyphens */
char dashes[65] = "----------------------------------------------------------------";

/* Used by add and remove; will return locaton of matching file's hash or version number depending on flag */
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
	/* First token will always be the project version number, useless for now */
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
				/* If the two hashes are equal, no need to put in new hash */
				return -1;
			} else {
				/* Will either return location of hash or location of file version number */
				return byte_count - (flag * check_bytes) - (flag * prev_bytes);
			}
		}
		if (strcmp(token, path) == 0) {
			/* If path found in .Manifest, check its hash, which will be next token */
			check = 1;
		}
		check_bytes = prev_bytes;
		prev_bytes = strlen(token) + 1;
		byte_count += prev_bytes;
	}
	return strlen(input);
}

/* Add a file to a .Manifest */
int add(int fd_manifest, char *hashcode, char *path, char *input, int flag) {
	int *vers = (int *) malloc(sizeof(int));
	*vers = 0;
	/* Flag is 1 for operations that might increment file's version number, like push or upgrade */
	/* Else, for operations like add, will be 0 */
	int move = tokenize(path, input, hashcode, flag, vers);
	if (move == 0) {
		fprintf(stderr, "ERROR: Could not read .Manifest.\n");
		return -1;
	}
	/* -1 means file's new hash matches hash already in .Manifest */
	if (move == -1 && !flag) {
		fprintf(stderr, "ERROR: File already up-to-date in .Manifest.\n");
		return -1;
	}

	lseek(fd_manifest, move, SEEK_SET);
	/* If file wasn't in .Manifest, automatically will be added to the end with a starting version of 0 */
	if (move == strlen(input)) {
		write(fd_manifest, "0\t", 2);
		write(fd_manifest, path, strlen(path));
		write(fd_manifest, "\t", 1);
		write(fd_manifest, hashcode, strlen(hashcode));	
		write(fd_manifest, "\n", 1);
	} else if (!flag) {
		/* If it was in .Manifest and flag is not set, just update hash */
		write(fd_manifest, hashcode, strlen(hashcode));
	} else {
		/* If it was and flag was set, increment file version number and update hash */
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
/* Remove a file from .Manifest by showing its hash code as a series of dashes */
int remover(int fd_manifest, char *path, char *input) {
	/* Will never have to update version number of file being removed, so flag is always 0 */
	int move = tokenize(path, input, dashes, 0, NULL);
	/* Don't do anything if file isn't in .Manifest */
	if (move == strlen(input)) {
		fprintf(stderr, "ERROR: File \"%s\" not in .Manifest file.", path);
		return -1;
	}
	if (move == -1) {
		fprintf(stderr, "ERROR: File \"%s\" already removed from \".Manifest\" file.\n", path);
		return -1;
	}
	/* Write dashes in place of hash */
	lseek(fd_manifest, move, SEEK_SET);
	write(fd_manifest, dashes, strlen(dashes));
	return 0;
}

/* Utility function: Check if directory exists */
int check_dir(char *path) {
	DIR *dr = opendir(path);
	if (dr == NULL) {
		closedir(dr);
		return -1;	
	}
	closedir(dr);
	return 0;
}

/* Remove a directory by recursively removing all of its files and subdirectories */
int remove_dir(char *path) {
	DIR *dr;
	size_t path_len = strlen(path);
	int ret = -1;
	if (!(dr = opendir(path))) {
		fprintf(stderr, "ERROR: Could not open project \"%s\" on server's side.\n", (path + 17));
		closedir(dr);
		return ret;
	}
	struct dirent *de;
	ret = 0;
	while ((de = readdir(dr)) != NULL) {
		/* Skip self and parent directory */
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}
		/* Create new path name with current file's/directory's name attached to end of given path */
		char *new_path = (char *) malloc(strlen(de->d_name) + strlen(path) + 2);
		if (new_path != NULL) {
			snprintf(new_path, strlen(de->d_name) + strlen(path) + 2, "%s/%s", path, de->d_name);
			/* If a directory, recurse */
			if (de->d_type == DT_DIR) {
				ret = remove_dir(new_path);
			} else {
				/* Else, remove the file */
				ret = remove(new_path);
			}
			if (ret < 0) {
				fprintf(stderr, "ERROR: Failed to delete \"%s\" from server. It is probably currently in use.\n", new_path);
				closedir(dr);
				return ret;
			}
		}
		free(new_path);
	}
	/* Finally, remove the directory itself */
	closedir(dr);
	ret = rmdir(path);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Failed to delete \"%s\" from server. It is probably currently in use.\n", path);
	}
	return ret;
}

/* Utility function: Get size of file */
int get_file_size(int fd) {
	struct stat st = {0};
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "ERROR: fstat() failed.\n");
		return -1;
	}
	return st.st_size;
}

/* Used by commit() below; return codes listed below:
 * -1 if error
 * 1 if path is present in .Manifest and hashes are different
 * 0 if path is present in .Manifest and hashes are same
 * 2 if path is not present in .Manifest */
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

/* Parse throough client's .Manifest and use commit_check() to determine if there are any differences
 * between client's .Manifest and server's */
int commit(int fd_comm, char *client_mani, char *server_mani) {	
	int len = strlen(client_mani);
	int j = 0, k = 0;
	char *token;
	int token_len = 0;
	int last_sep = 0;
	int count = -1;
	int version = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
	char *path = NULL;
	int delete_check = 0;
	for (j = 0; j < len; ++j) {
		/* Tokenize by creating a new token whenever whitespace is encountered */
		if (client_mani[j] != '\t' && client_mani[j] != '\n') {
			++token_len;
			continue;
		} else {
			token = (char *) malloc(token_len + 1);
			for (k = 0; k < token_len; ++k) {
				token[k] = client_mani[last_sep + k];
			}
			token[token_len] = '\0';
			last_sep += token_len + 1;
			token_len = 0;
			++count;
		}
		if (count == 0) {
			free(token);
			continue;
		}
		if (count % 3 == 1) {
			/* New version of file */
			version = atoi(token);
			++version;
			free(token);
		} else if (count % 3 == 2) {	
			/* File's path */
			int fd = open(token, O_RDONLY);
			int size = get_file_size(fd);
			path = (char *)malloc(strlen(token) + 1);
			strcpy(path, token);
			path[strlen(token)] = '\0';
			if (fd < 0 || size < 0) {
				delete_check = 1;	
				close(fd);
				continue;	
			}
			char buff[size + 1];
			read(fd, buff, size);
			buff[size] = '\0';
			SHA256(buff, strlen(buff), hash);
			/* Get file's hash too */
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}
			free(token);
			close(fd);
		} else {
			int token_equals_dashes = strcmp(token, dashes);
			int comm_check;
			/* Sometimes will try to open a file that doesn't exist anymore; have to check if that file
			 * was removed from .Manifest */
			if (token_equals_dashes != 0 && delete_check) {
				free(path);
				return -1;
			} else if (token_equals_dashes == 0) {
				/* Run commit_check() to see if file was removed from .Manifest already */
				delete_check = 0;
				strcpy(hashed, dashes);
				comm_check = commit_check(version - 1, path, dashes, server_mani);
			} else {
				comm_check = commit_check(version - 1, path, hashed, server_mani);
			}
			if (comm_check == -1) {
				return -1;
			}
			if (token_equals_dashes == 0 && comm_check == 1) {
				/* If file is present and its hash isn't dashes, and it was removed from local .Manifest, D*/
				write(fd_comm, "D\t", 2);
			} else if (token_equals_dashes != 0 && comm_check == 2) {
				/* If file wasn't removed from local .Manifest and it's not present, A */
				write(fd_comm, "A\t", 2);
			} else if (token_equals_dashes != 0 && comm_check == 1) {
				/* If file wasn't removed from local .Manifest, it's present in server's .Manifest, and
				 * the hashes aren't the same, M */
				write(fd_comm, "M\t", 2);
			} else {
				continue;
			}
			/* If code was given, write this into .Commit */
			char *version_string = (char *) malloc(sizeof(version) + 1);
			snprintf(version_string, sizeof(version) + 1, "%d", version);
			write(fd_comm, version_string, strlen(version_string));
			write(fd_comm, "\t", 1);
			write(fd_comm, path, strlen(path));
			write(fd_comm, "\t", 1);
			write(fd_comm, hashed, strlen(hashed));
			write(fd_comm, "\n", 1);
			free(version_string);
			free(token);
		}
	}
	return 1;	
}

/* Get rid of all .Commits that don't match arg name */
int delete_commits(char *proj_path, char *name) {
	DIR *dir;
	if (!(dir = opendir(proj_path))) {
		fprintf(stderr, "ERROR: Could not open \"%s\" on server.\n", proj_path);
		closedir(dir);
		return -1;
	}
	struct dirent *de;
	/* Open .server_directory/proj_path */
	while ((de = readdir(dir)) != NULL) {
		/* If .Commit is part of the name, delete te file */
		if (strstr(de->d_name, ".Commit") != NULL && strcmp(de->d_name, name) != 0) {

			char *comm_path = (char *) malloc(strlen(proj_path) + strlen(de->d_name) + 1);

			snprintf(comm_path, strlen(proj_path) + strlen(de->d_name) + 1, "%s%s", proj_path, de->d_name);
			if (de->d_type != DT_DIR) {
				remove(comm_path);
			}
			free(comm_path);
		}
	}
	closedir(dir);
	return 0;
}

/* Check if .Commit given from client matches any on server */
int push_check(char *project, char *comm_input) {
	char path[20 + strlen(project)];
	snprintf(path, strlen(project) + 20, ".server_directory/%s/", project);
	DIR *dir;
	if (!(dir = opendir(path))) {
		fprintf(stderr, "ERROR: Could not open project \"%s\" on server.\n", project);
		closedir(dir);
		return -1;
	}
	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		/* Check if file's name contains .Commit */
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
			/* Get input of file */
			char input[size + 1];
			int bytes_read = read(fd_comm, input, size);
			input[size] = '\0';
			input[size - 1] = '\0';
			/* Check if file's input matches given .Commit's */
			if (strcmp(input, comm_input) == 0) {
				free(comm_path);
				close(fd_comm);
				/* Delete all other .Commits */
				int ret = delete_commits(path, de->d_name);
				closedir(dir);
				return ret;
			}
			close(fd_comm);
			free(comm_path);
		}
	}
	closedir(dir);
	return 1;
}

/* Copy src directory into dest directory recursively */
int dir_copy(char *src, char *dest) {
	DIR *dir;
	struct dirent *de;
	if ((dir = opendir(src)) == NULL) {
		fprintf(stderr, "ERROR: Cannot open directory \"%s\".\n", src);
		closedir(dir);
		return -1;
	}
	while ((de = readdir(dir)) != NULL) {
		/* Skip self and parent directory */
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
			continue;
		}
		char *new_src_path = (char *) malloc(strlen(src) + strlen(de->d_name) + 2);
		char *new_dest_path = (char *) malloc(strlen(dest) + strlen(de->d_name) + 2);
		snprintf(new_src_path, strlen(src) + strlen(de->d_name) + 2, "%s/%s", src, de->d_name);
		snprintf(new_dest_path, strlen(dest) + strlen(de->d_name) + 2, "%s/%s", dest, de->d_name);
		/* If de is a directory, create a new matching one in dest and fill it recursively */
		if (de->d_type == DT_DIR) {
			mkdir(new_dest_path, 0744);
			if (dir_copy(new_src_path, new_dest_path) != 0) {
				return -1;
			} else {
				return 0;
			}
		} else {
			/* Otherwise, just copy src's file's contents into newly created dest's file */
			int fd_src_file = open(new_src_path, O_RDONLY);
			if (fd_src_file < 0) {
				fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", new_src_path);
				close(fd_src_file);
				closedir(dir);
				return -1;
			}
			int fd_dest_file = open(new_dest_path, O_CREAT | O_WRONLY, 0744);
			if (fd_dest_file < 0) {
				fprintf(stderr, "ERROR: Cannot create file \"%s\".\n", new_dest_path);
				close(fd_dest_file);
				close(fd_src_file);
				closedir(dir);
				return -1;
			}
			int size = get_file_size(fd_src_file);
			if (size < 0) {
				fprintf(stderr, "ERROR: Cannot get size of file \"%s\".\n", new_src_path);
				close(fd_dest_file);
				close(fd_src_file);
				closedir(dir);
				return -1;
			}
			char input[size + 1];
			read(fd_src_file, input, size);
			input[size] = '\0';
			write(fd_dest_file, input, size);
			close(fd_dest_file);
			close(fd_src_file);
		}
		free(new_src_path);
		free(new_dest_path);
	}
	closedir(dir);
	return 0;
}

/* Similar to commit_check(), with following codes:
 * 1 for U
 * 2 for M
 * 3 for A
 * 4 for D
 * All pathways to these codes are exactly as described in project prompt */
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
			mani_token[token_len] = '\0';
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
				if (strcmp(hash, dashes) == 0 && strcmp(mani_token, dashes) == 0) {
					return 5;
				} else if (strcmp(hash, dashes) == 0 && strcmp(mani_token, dashes) != 0 && client_version != server_version) {
					return 3;
				} else if (strcmp(hash, dashes) != 0 && strcmp(mani_token, dashes) == 0) {
					if (client_version == server_version) {
						return 4;
					} else {
						return 1;
					}
				} else if (strcmp(hash, mani_token) == 0 && file_version == version && client_version == server_version) {
					free(mani_token);
					return 2;
				} else if (strcmp(hash, mani_token) != 0 && client_version == server_version) {
					free(mani_token);
					return 1;
				} else if (strcmp(hash, mani_token) != 0 && client_version != server_version && file_version != version) {
					free(mani_token);
					return -1;
				}
				free(mani_token);
				break;
			} 
		}
	}
	/* Repeat for final token missed by for loop */
	if (check == 1 && token_len > 0) {
		mani_token = (char *) malloc(token_len + 1);
		for (j = 0; j < token_len; ++j) {
			mani_token[j] = mani[last_sep + 1];
		}
		mani_token[token_len] = '\0';
		if (strcmp(hash, dashes) == 0 && strcmp(mani_token, dashes) == 0) {
				return 5;
		} else if (strcmp(hash, dashes) == 0 && strcmp(mani_token, dashes) != 0 && client_version != server_version) {
				return 3;
		} else if (strcmp(hash, dashes) != 0 && strcmp(mani_token, dashes) == 0) {
			if (client_version == server_version) {
				return 4;
			} else {
				return 1;
			}
		} else if (strcmp(hash, mani_token) == 0 && file_version == version && client_version == server_version) {
			free(mani_token);
			return 2;
		} else if (strcmp(hash, mani_token) != 0 && client_version == server_version) {
			free(mani_token);
			return 1;
		} else if (strcmp(hash, mani_token) != 0 && client_version != server_version && file_version != version) {
			free(mani_token);
			return -1;
		}
	}
	if (client_version == server_version) {
		return 1;
	} else {
		return 4;
	}
}

/* Fill .Update with the help of update_check() */
int update(int fd_upd, char *client_mani, char *server_mani, int client_version, int server_version) {
	/* Tokenize client's .Manifest */
	int count = -1;
	char *mani_token;
	int j = 0, k = 0;
	int last_sep = 0;
	int token_len = 0;
	int len = strlen(client_mani);
	int version = 0;
	int print = 1;
	char hashed[SHA256_DIGEST_LENGTH * 2 + 1];
	char *path = NULL;
	for (j = 0; j < len; ++j) {
		if (client_mani[j] != '\t' && client_mani[j] != '\n') {
				++token_len;
				continue;
		} else {
			mani_token = (char *) malloc(token_len + 1);
			for (k = 0; k < token_len; ++k) {
				mani_token[k] = client_mani[last_sep + k];
			}
			mani_token[token_len] = '\0';
			last_sep += token_len + 1;
			token_len = 0;
			++count;
		}
		/* First token will always be the .Manifest version number, which is already given in args */
		if (count == 0) {
			free(mani_token);
			continue;
		}
		if (count % 3 == 1) {
			/* File version */
			version = atoi(mani_token);
		} else if (count % 3 == 2) {
			/* Path */
			int fd = open(mani_token, O_RDONLY);
			int size = get_file_size(fd);
			if (fd < 0 || size < 0) {
				fprintf(stderr, "Cannot read \"%s\".", mani_token);
				return -1;
			}
			char buffer[size + 1];
			read(fd, buffer, size);
			buffer[size] = '\0';
			/* Get hash too */
			unsigned char hash[SHA256_DIGEST_LENGTH];
			SHA256(buffer, strlen(buffer), hash);
			int i = 0;
			for (i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
				sprintf(hashed + (i * 2), "%02x", hash[i]);
			}
			hashed[SHA256_DIGEST_LENGTH * 2] = '\0';
			path = (char *) malloc(strlen(mani_token) + 1);
			strcpy(path, mani_token);
			path[strlen(mani_token)] = '\0';
			close(fd);
			free(mani_token);
		} else {
			int upd_check = 0;
			/* Check if file was already deleted in server */
			if (strcmp(mani_token, dashes) == 0) {
				upd_check = update_check(server_mani, client_version, server_version, dashes, version, path);
			} else {
				/* Or just do a regular check */
				upd_check = update_check(server_mani, client_version, server_version, hashed, version, path);
			}	
			/* If conflict, stop printing anything else and report conflict */
			if (upd_check == -1) {
				print = 0;
				printf("CONFLICT: %s\n", path);
			}
			if (print) {
				/* Skip any U codes */
				if (upd_check == 2) {
					write(fd_upd, "M\t", 2);
					printf("M\t");
				} else if (upd_check == 3) {
					write(fd_upd, "A\t", 2);
					printf("A\t");
				} else if (upd_check == 4) {
					write(fd_upd, "D\t", 2);
					printf("D\t");
				}
 				if (upd_check > 1 && upd_check < 5) {
					char vers[sizeof(version) + 1];
					snprintf(vers, sizeof(version), "%d", version);
					vers[sizeof(version)] = '\0';
					write(fd_upd, vers, strlen(vers));
					write(fd_upd, "\t", 1);
					write(fd_upd, path, strlen(path));
					write(fd_upd, "\t", 1);
					write(fd_upd, hashed, strlen(hashed));
					write(fd_upd, "\n", 1);
					printf("%d\t%s\t%s\n", version, path, hashed);	
				}
			}
			free(path);
			free(mani_token);
		}
	}
	/* Also need to check for files that are on server that just aren't on client's .Manifest at all, even as removed */
	if (print) {
		count = -1;
		mani_token = NULL;
		last_sep = 0;
		token_len = 0;
		len = strlen(server_mani);
		version = 0;
		path = NULL;
		for (j = 0; j < len; ++j) {
			if (server_mani[j] != '\t' && server_mani[j] != '\n') {
					++token_len;
					continue;
			} else {
				mani_token = (char *) malloc(token_len + 1);
				for (k = 0; k < token_len; ++k) {
					mani_token[k] = server_mani[last_sep + k];
				}
				mani_token[token_len] = '\0';
				last_sep += token_len + 1;
				token_len = 0;
				++count;
			}
			/* First token will always be the .Manifest version number, which is already given in args */
			if (count == 0) {
				free(mani_token);
				continue;
			}
			if (count % 3 == 1) {
			/* File version */
				version = atoi(mani_token);
			} else if (count % 3 == 2) {
			/* Path */
			path = (char *) malloc(strlen(mani_token) + 1);
			strcpy(path, mani_token);
			path[strlen(mani_token)] = '\0';
			free(mani_token);
			} else {
				int upd_check = update_check(client_mani, client_version, server_version, mani_token, version, path);
				/* Only need to check for code 4: path not found in other mani and versions different */
				if (strcmp(mani_token, dashes) != 0 && upd_check == 4) {
					write(fd_upd, "A\t", 2);
					printf("A\t");
					char vers[sizeof(version) + 1];
					snprintf(vers, sizeof(version), "%d", version);
					vers[sizeof(version)] = '\0';
					write(fd_upd, vers, strlen(vers));
					write(fd_upd, "\t", 1);
					write(fd_upd, path, strlen(path));
					write(fd_upd, "\t", 1);
					write(fd_upd, hashed, strlen(mani_token));
					write(fd_upd, "\n", 1);
					printf("%d\t%s\t%s\n", version, path, mani_token);	
				}
			}
			free(path);
			free(mani_token);
		}
	}	
	return print;
}

/* Look for matching version folder, and delete any version folder with greater version # */
int rollback(char *path, int version) {
	DIR *dir;
	if (!(dir = opendir(path))) {
		fprintf(stderr, "ERROR: Could not open \"%s\" on server.\n", path);
		closedir(dir);
		return -1;
	}
	struct dirent *de;
	while ((de = readdir(dir)) != NULL) {
		char temp[strlen(de->d_name) + 1];
		strcpy(temp, de->d_name);
		temp[strlen(de->d_name)] = '\0';
		if (strstr(temp, "version") != NULL) {
			/* Skip any non-directories */
			if (de->d_type != DT_DIR) {
				continue;
			}
			char vers_path[strlen(path) + strlen(de->d_name) + 2];
			snprintf(vers_path, strlen(path) + strlen(de->d_name) + 2, "%s/%s", path, de->d_name);
			char mani_path[strlen(vers_path) + 12];
			snprintf(mani_path, strlen(vers_path) + 12, "%s/.Manifest", vers_path);
			int fd_mani = open(mani_path, O_RDONLY);
			if (fd_mani < 0) {
				fprintf(stderr, "ERROR: Not able to parse through versions.\n");
				close(fd_mani);
				closedir(dir);
				return -1;
			}
			char mani_input[256];
			read(fd_mani, mani_input, 255);
			/* Get the version number for the current de */
			char *vers_token = strtok(mani_input, "\n");
			int this_vers = atoi(vers_token);
			/* If version number is greater, delete that directory */
			if (this_vers > version) {
				remove_dir(vers_path);
			}
			close(fd_mani);
		}
	}
	closedir(dir);
	return 0;
}
