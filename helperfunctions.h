/**********************************
 * Aditi Singh and Rumeet Goradia 
 * as2811, rug5
 * Section 04
 * CS214
 **********************************/

int add(int fd_manifest, char *hashcode, char *path, char *input, int flag);
int remover(int fd_manifest, char *path, char *input);
int check_dir(char *path);
int remove_dir(char *path);
int commit(int fd_comm, char *client_mani, char *server_mani, int fd_mani);
int push_check(char *project, char *comm_input);
int dir_copy(char *src, char *dest, int flag);
int update(int fd_upd, char *client_mani, char *server_mani, int client_version, int server_version);
int rollback(char *path, int version);
int create_dirs(char *file_path, char *parent, int flag);
