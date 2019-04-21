typedef struct node {
	char *token;
	struct node *next;
} node;

int add(char *path_mani, char *hashcode, char *path, char *input);
int remover(char *path_mani, char *path, char *input);
