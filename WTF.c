#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

int main (int argc, char **argv) {
	if (argc < 2) {
		printf("ERROR: Not enough arguments.\n");
		return EXIT_FAILURE;
	}
	if (strcmp("configure", argv[1]) == 0) {
		if (argc < 4) {
			printf("ERROR: Need IP address and port number.\n");
			return EXIT_FAILURE;
		} else if (argc > 4) {
			printf("ERROR: Too many arguments.\n");
			return EXIT_FAILURE;
		}
		int conf_file = open("./.configure", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (conf_file < 0) {
			printf("ERROR: Could not open or create .configure file.\n");
			return EXIT_FAILURE;
		}
		write(conf_file, argv[2], strlen(argv[2]));
		write(conf_file, "\n", 1);
		write(conf_file, argv[3], strlen(argv[3]));
		write(conf_file, "\n", 1);
		close(conf_file);
	}
	return EXIT_SUCCESS;
}
