int add(int fd_manifest, char *hashcode, char *path, char *input);
int remover(int fd_manifest, char *path, char *input);
int check_dir(char *path);
int remove_dir(char *path);
int commit(int fd_comm, char *client_mani, char *server_mani, int fd_mani);
