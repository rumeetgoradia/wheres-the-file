#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "clientfunctions.h"

node *head = NULL;

int insert_list(char *token) {
	node *temp;
	node *ptr;
	temp = (node *) malloc(sizeof(node));
	if (temp == NULL) {
		printf("Insufficient memory.\n");
		return -1;
	}
	temp->freq = 1;
	temp->token = (char *) malloc(strlen(token) + 1);
	strncpy(temp->token, token, (strlen(token)));
	/* Case 1: New list */
	if (head == NULL) {
		head = temp;
		temp->next = NULL;
		return 1;
	}
	/* Case 2: Only one node */
	ptr = head;
	while (ptr->next != NULL) {
		ptr = ptr->next;
	}
	ptr->next = temp;
	temp->next = NULL;
	return 1;
}

/* Separate input into tokens and return how many unique tokens were registerd */
unsigned int tokenize(char *input) {
	if (input == NULL || strlen(input) == 0) {
		return 0;
	}
	int i = 0, j = 0;
	int pos_last_sep = 0;
	int token_len = 0;
	int length = strlen(input);
	int last_was_sep = 1;
	int current_sep = 1;
	int count = 0;
	/* Traverse through entire input string, keeping track of unique tokens with count */
	for (i = 0; i < length; i++) {
		/* If current char is not a delimiter, increase the length of the token */
		if (input[i] != '\t' && input[i] != '\n') {
			++token_len;
			last_was_sep = 0;
			current_sep = 0;
		/* If current char is a delimiter and previous char wasn't, create token */
		} else if (last_was_sep == 0) {
			char *token = (char *) malloc(sizeof(char) * (token_len + 1));
			if (token == NULL) {
				printf("Error: Insufficient memory.\n");
				return -1;
			}
			for (j = 0; j < token_len; ++j) {
				token[j] = input[pos_last_sep + j];
			}
			token[token_len] = '\0';
			/* Change index of most recent delimiter for next token creation */
			pos_last_sep += token_len + 1;
			int inc = insert_list(token);
			if (inc == -1) {
				return -1;
			}
			++count;
			last_was_sep = 1;
			token_len = 0;
			current_sep = 1;
		/* If current char is a delimiter and previous char was also, do not create token 
 *  		 * and update position of last delimiter */ 
		} else {
			++pos_last_sep;
			current_sep = 1;
		}
		/* If current char is a delimiter, enter it into linked list of tokens */
		if (current_sep == 1) {
			int inc = 0;
			/* Instead of inputting whitespace, enter unique string that corresponds
 *  			 * to each different type of whitespace */ 
			char temp[2];
			temp[0] = input[i];
			inc = insert_list(temp);
			if (inc == -1) {
				return -1;
			}
			count += inc;
		}
	}
	/* If final char isn't a delimiter, create a new token out of remaining chars */
	/*if (token_len > 0) {
		char * token = (char *)malloc(sizeof(char) * (token_len + 1));
		if (token == NULL) {
			printf("Error: Insufficient memory.\n");
			return -1;
		}
		for (j = 0; j < token_len; ++j) {
			token[j] = input[pos_last_sep + j];
		}
		token[token_len] = '\0';
		int inc = insert_list(token);
		if (inc == -1) {
			return -1;
		}
		count += inc;
	} */
	return count;
}

void add(int fd_manifest, char *hashcode, char *path, char *input) {
	tokenize(input);
	node *ptr = head;
	int check = 0;
	int written = 0;
	while (ptr!= NULL) {
		if (check == 1 && strcmp(ptr->token, "\t") != 0 && strcmp(ptr->token, path) != 0) {
			if (strcmp(hashcode, ptr->token) != 0) {
				ptr->token = (char *) malloc(strlen(hashcode) + 1);
				strcpy(ptr->token, hashcode);
			}
			check = 0;
		}
		if (ptr->next != NULL && ptr->next->next != NULL && strcmp(ptr->next->next->token, path) == 0) {
			check = 1;
			int version = atoi(ptr->token);
			++version;
			if (version % 10 == 0) {
				ptr->token = (char *) malloc(sizeof(version) / sizeof(char) + 1);
			}
			sprintf(ptr->token, "%d", version);
			written = 1;
		}
		write(fd_manifest, ptr->token, strlen(ptr->token));
		ptr = ptr->next;
	}
	if (!written) {
		write(fd_manifest, "0\t", 2);
		write(fd_manifest, path, strlen(path));
		write(fd_manifest, "\t", 1);
		write(fd_manifest, hashcode, strlen(hashcode));
		write(fd_manifest, "\n", 1);
	}
}
