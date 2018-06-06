#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/select.h>

void usage()
{
	fprintf(stderr, "usage: %s [-f gpiointrdev] [-s]\n", getprogname());
}

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpiointr0";
	bool loop = true;
	int fd;
	fd_set readfds;
	ssize_t res;

	while ((ch = getopt(argc, argv, "f:s")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 's':
			loop = false;
			break;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}
	argv += optind;
	argc -= optind;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	do {

		res = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot select() %s", file);
		} else if (res == 0) {
			printf("%s: select() timed out on %s", getprogname(), file);
		} else {
			printf("%s: select() returned %i on %s\n", getprogname(), res, file);
		}

	} while (loop);

	return EXIT_SUCCESS;
}
