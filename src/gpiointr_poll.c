#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/poll.h>

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpiointr0";
	bool loop = true;
	int fd;
	struct pollfd fds;
	int res;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 's':
			loop = false;
			break;
		default:
			printf("Invalid usage.");
			return EXIT_FAILURE;
		}
	}
	argv += optind;
	argc -= optind;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);


	fds.fd = fd;
	fds.events = POLLIN | POLLRDNORM;
	fds.revents = 0;

	do {

		res = poll(&fds, 1, INFTIM);
		if (res < 0)
			err(EXIT_FAILURE, "Cannot poll %s", file);

		printf("%s: poll returned %i (%hi) on %s\n", getprogname(), res, fds.revents, file);

	} while (loop);

	return EXIT_SUCCESS;
}
