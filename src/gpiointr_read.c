#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpiointr0";
	int fd;
	char buffer[1024];
	ssize_t res;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
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

	res = read(fd, buffer, sizeof(buffer));
	if(res < 0)
		err(EXIT_FAILURE, "Cannot read from %s", file);

	printf("%s: Read %i bytes from %s\n", getprogname(), res, file);
	return EXIT_SUCCESS;
}
