#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/gpio.h>

void usage()
{
	fprintf(stderr, "Usage: %s [-f gpiointrdev] [-r]\n", getprogname());
}

int main(int argc, char *argv[])
{
	int ch;
	bool reset = false;
	char *file = "/dev/gpiointr0";
	int fd;
	int res;
	unsigned int counter;

	while ((ch = getopt(argc, argv, "f:r")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 'r':
			reset = true;
			break;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}
	argv += optind;
	argc -= optind;

	fd = open(file, O_RDONLY);
	if(fd == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);

	if (reset) {

		res = ioctl(fd, GPIOINTRRESETCOUNTER);
		if(res < 0)
			err(EXIT_FAILURE, "interrupt counter reset of %s failed\n", file);

		printf("%s: interrupt counter of %s successfully reset\n", getprogname(), file);
		return EXIT_SUCCESS;

	} else {
		res = ioctl(fd, GPIOINTRGETCOUNTER, &counter);
		if(res < 0)
			err(EXIT_FAILURE, "retrieving interrupt counter of %s failed", file);

		printf("%s: interrupt counter of %s: %u\n", getprogname(), file, counter);
		return EXIT_SUCCESS;
	}

}
