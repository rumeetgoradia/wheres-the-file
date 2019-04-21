#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include<unistd.h>
#include<fcntl.h>
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

void clear_list(node *head) {
	node *ptr = head;
	if (ptr->next == NULL) {
		free(ptr->token);
		ptr = NULL;
	} else {
		clear_list(ptr->next);
		free(ptr->token);
		ptr = NULL;
	}
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
	return count;
}

int add(char *path_mani, char *hashcode, char *path, char *input) {
	tokenize(input);
	node *ptr = head;
	int written = 0;
	int fd_manifest = open(path_mani, O_WRONLY, 0644);
	if (fd_manifest < 0) {
		printf("ERROR: Could not open \".Manifest\" file.\n");
		return -1;  
	}
	while (ptr!= NULL) {
		if (ptr->next != NULL && ptr->next->next != NULL && ptr->next->next->next != NULL && ptr->next->next->next->next != NULL 
		   && strcmp(ptr->next->next->token, path) == 0 && strcmp(ptr->next->next->next->next->token, hashcode) != 0) {
			int version = atoi(ptr->token);
			++version;
			if (version % 10 == 0) {
				ptr->token = (char *) malloc(sizeof(version) / sizeof(char) + 1);
			}
			sprintf(ptr->token, "%d", version);
			ptr->next->next->next->next->token = (char *) malloc(strlen(hashcode) + 1);
			strcpy(ptr->next->next->next->next->token, hashcode);
		}
		if (strcmp(ptr->token, path) == 0) {
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
	clear_list(head);
	return 0;
}
