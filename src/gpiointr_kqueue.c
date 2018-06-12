#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

//#include <sys/types.h>
#include <sys/event.h>

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
	struct kevent event[1];
	struct kevent tevent[1];
	int kq = -1;
	int nev = -1;

	while ((ch = getopt(argc, argv, "f:s")) != -1) {
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

	kq = kqueue();
	if (fd == -1)
		err(EXIT_FAILURE, "kqueue() %s", file);

	EV_SET(&event[0], fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	nev = kevent(kq, event, 1, NULL, 0, NULL);
	if (nev == -1)
		err(EXIT_FAILURE, "kevent() %s", file);

	do {

		nev = kevent(kq, NULL, 0, tevent, 1, NULL);
		if (nev == -1) {
			err(EXIT_FAILURE, "kevent() %s", file);
		} else if (nev == 0) {
			printf("%s: kevent() timed out on %s\n", getprogname(), file);
		} else {
			printf("%s: kevent() returned %i events (flags: %d) on %s\n", getprogname(), nev, tevent[0].flags, file);
			if (tevent[0].flags & EV_EOF) {
				return EXIT_FAILURE;
			}
		}

	} while (loop);

	return EXIT_SUCCESS;
}
