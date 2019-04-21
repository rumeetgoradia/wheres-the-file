typedef struct node {
	char *string;
	struct node *next;
} node;

void add(int fd_manifest, char *hashcode, char *path);
