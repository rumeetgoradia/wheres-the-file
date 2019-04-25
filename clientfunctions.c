#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "clientfunctions.h"

/* node *head = NULL;

int insert_list(char *token) {
	node *temp;
	node *ptr;
	temp = (node *) malloc(sizeof(node));
	if (temp == NULL) {
		printf("Insufficient memory.\n");
		return -1;
	}
	temp->token = (char *) malloc(strlen(token) + 1);
	strncpy(temp->token, token, (strlen(token)));
	 Case 1: New list 
	if (head == NULL) {
		head = temp;
		temp->next = NULL;
		return 1;
	}
 Case 2: Only one node 
	ptr = head;
	while (ptr->next != NULL) {
		ptr = ptr->next;
	}
	ptr->next = temp;
	temp->next = NULL;
	return 1;
}

void clear_list(node *head) {
	if (head != NULL) {
		node *ptr = head;
		if (ptr->next == NULL) {
			if (ptr != NULL && ptr->token != NULL && strlen(ptr->token) > 0) {
				free(ptr->token);
				ptr = NULL;
			}
		} else {
			clear_list(ptr->next);
			if (ptr != NULL && ptr->token != NULL && strlen(ptr->token) > 0) {
				free(ptr->token);
				ptr = NULL;
			}
		}
	}
} */

// /* Separate input into tokens and return how many unique tokens were registerd */
// unsigned int tokenize(char *input) {
// 	if (input == NULL || strlen(input) == 0) {
// 		return 0;
// 	}
// 	int i = 0, j = 0;
// 	int pos_last_sep = 0;
// 	int token_len = 0;
// 	int length = strlen(input);
// 	int last_was_sep = 1;
// 	int current_sep = 1;
// 	int count = 0;
// 	/* Traverse through entire input string, keeping track of unique tokens with count */
// 	for (i = 0; i < length; i++) {
// 		/* If current char is not a delimiter, increase the length of the token */
// 		if (input[i] != '\t' && input[i] != '\n') {
// 			++token_len;
// 			last_was_sep = 0;
// 			current_sep = 0;
// 		/* If current char is a delimiter and previous char wasn't, create token */
// 		} else if (last_was_sep == 0) {
// 			char *token = (char *) malloc(sizeof(char) * (token_len + 1));
// 			if (token == NULL) {
// 				printf("Error: Insufficient memory.\n");
// 				return -1;
// 			}
// 			for (j = 0; j < token_len; ++j) {
// 				token[j] = input[pos_last_sep + j];
// 			}
// 			token[token_len] = '\0';
// 			/* Change index of most recent delimiter for next token creation */
// 			pos_last_sep += token_len + 1;
// 			int inc = insert_list(token);
// 			if (inc == -1) {
// 				return -1;
// 			}
// 			++count;
// 			last_was_sep = 1;
// 			token_len = 0;
// 			current_sep = 1;
// 		/* If current char is a delimiter and previous char was also, do not create token 
//  *  		 * and update position of last delimiter */ 
// 		} else {
// 			++pos_last_sep;
// 			current_sep = 1;
// 		}
// 		/* If current char is a delimiter, enter it into linked list of tokens */
// 		if (current_sep == 1) {
// 			int inc = 0;
// 			/* Instead of inputting whitespace, enter unique string that corresponds
//  *  			 * to each different type of whitespace */ 
// 			char temp[2];
// 			temp[0] = input[i];
// 			inc = insert_list(temp);
// 			if (inc == -1) {
// 				return -1;
// 			}
// 			count += inc;
// 		}
// 	}
// 	return count;
// }

unsigned int tokenize(char *path, char *input, int *version, int flag, char *hash) {
	if (input == NULL) {
		return 0;	
	}
	char whole_string[strlen(input)];
	strcpy(whole_string, input);
	printf("whole_string: %s\n", whole_string);
	char *token;
	unsigned int byte_count = 0;
	unsigned int prev_bytes = 0;
	unsigned int check_bytes = 0;
	short check = 0;
	token = strtok(whole_string, "\n\t");
	printf("first token: %s\n", token);
	prev_bytes = strlen(token) + 1;
	byte_count += prev_bytes;
	if (version != NULL) {
		*version = 0;
	}
	while (token != NULL) {
		printf("in while loop\n");
		if (version != NULL) {
			if (token[0] == 'R') {
				*version = 0;	
			} else {
				*version = atoi(token);
			}
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
		if (strcmp(token, path) == 0 && !flag) {
			return byte_count - prev_bytes;
		} else if (strcmp(token, path) == 0) {
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
	int move = tokenize(path, input, version, 1, hashcode);
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
	int move = tokenize(path, input, version, 0, NULL);
	if (move == strlen(input)) {
		fprintf(stderr, "ERROR: File \"%s\" not in \".Manifest\" file.\n", path);
		return -1;
	}
	char buff[move];
	read(fd_manifest, buff, move);
	write(fd_manifest, "R", 1);
	return 0;
}
/*int remover(char *path_mani, char *path, char *input) {
	if (tokenize(input) == -1) {
		printf("ERROR: Could not read \".Manifest\" file.\n");
		clear_list(head);
		return -1;
	}
	node *ptr = head;
	int removed = 0;
	int fd_manifest = open(path_mani, O_WRONLY | O_TRUNC, 0644);
	if (fd_manifest < 0) {
		printf("ERROR: Could not open \".Manifest\" file.\n");
		clear_list(head);
		return -1;
	}
	while (ptr != NULL) {
		if (ptr->next != NULL && ptr->next->next != NULL && strcmp(ptr->next->next->token, path) == 0) {
			ptr->token = "";
			ptr->next->token = "";
			ptr->next->next->token = "";
			ptr->next->next->next->token = "";
			ptr->next->next->next->next->token = "";
			ptr->next->next->next->next->next->token = "";
			removed = 1;
		}
		write(fd_manifest, ptr->token, strlen(ptr->token));
		ptr = ptr->next;
	}
	close(fd_manifest);
	clear_list(head);
	if (!removed) {
		printf("ERROR: \"%s\" not in \".Manifest\" file.\n", path);
		return -1;
	}
	return 0;
} */
