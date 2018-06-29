#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <aio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

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
	char buffer[1024];
	struct aiocb iocb;
	int res;

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

	bzero(&iocb, sizeof(iocb));

	iocb.aio_fildes = fd;
	iocb.aio_nbytes = 1;
	iocb.aio_offset = 0;
	iocb.aio_buf = buffer;

	do {

		res = aio_read(&iocb);
		if (res < 0)
			err(EXIT_FAILURE, "Cannot aio_read from %s", file);

		do {
			res = aio_error(&iocb);
		} while (res == EINPROGRESS);

		if (res != 0)
			err(EXIT_FAILURE, "aio_error on %s", file);

		res = aio_return(&iocb);
		if(res < 0)
			err(EXIT_FAILURE, "aio_return on %s", file);

		printf("%s: Asynchronously read %i bytes from %s\n", getprogname(), res, file);

	} while (loop);

	return EXIT_SUCCESS;
}
