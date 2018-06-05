#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/poll.h>

void usage()
{
	fprintf(stderr, "usage: %s [-f gpiointrdev] [-s] [-t timeout]\n", getprogname());
}

static const char *poll_event_to_str(short event)
{
	switch (event) {
	case POLLIN:
		return "POLLIN";
	case POLLPRI:
		return "POLLPRI:";
	case POLLOUT:
		return "POLLOUT:";
	case POLLRDNORM:
		return "POLLRDNORM";
	case POLLRDBAND:
		return "POLLRDBAND";
	case POLLWRBAND:
		return "POLLWRBAND";
	case POLLINIGNEOF:
		return "POLLINIGNEOF";
	case POLLERR:
		return "POLLERR";
	case POLLHUP:
		return "POLLHUP";
	case POLLNVAL:
		return "POLLNVAL";
	default:
		return "unknown event";
	}
}

void print_events(short event)
{
	short curr_event = 0;
	bool first = true;

	for (int i = 0; i <= sizeof(short) * CHAR_BIT - 1; i++) {
		curr_event = 1 << i;
		if ((event & curr_event) == 0)
			continue;
		if (!first) {
			printf(" | ");
		} else {
			first = false;
		}
		printf("%s", poll_event_to_str(curr_event));
	}
}

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpiointr0";
	bool loop = true;
	int timeout = INFTIM;
	int fd;
	struct pollfd fds;
	int res;

	while ((ch = getopt(argc, argv, "f:st:")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 's':
			loop = false;
			break;
		case 't':
			errno = 0;
			timeout = strtol(optarg, NULL, 10);
			if (errno != 0) {
				warn("Invalid time out value");
				usage();
				 return EXIT_FAILURE;
			}
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

	fds.fd = fd;
	fds.events = POLLIN | POLLRDNORM;
	fds.revents = 0;

	do {

		res = poll(&fds, 1, timeout);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot poll() %s", file);
		} else if (res == 0) {
			printf("%s: poll() timed out on %s\n", getprogname(), file);
		} else {
			printf("%s: poll() returned %i (revents: ", getprogname(), res);
			print_events(fds.revents);
			printf(") on %s\n", file);
			if (fds.revents & (POLLHUP | POLLERR)) {
				return EXIT_FAILURE;
			}
		}

	} while (loop);

	return EXIT_SUCCESS;
}
