typedef struct node {
	char *token;
	struct node *next;
} node;

int add(int fd_manifest, char *hashcode, char *path, char *input);
int remover(int fd_manifest, char *path, char *input);
