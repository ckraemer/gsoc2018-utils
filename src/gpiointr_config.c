#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/gpio.h>

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpiointr0";
	int fd;
	int res;
	struct gpio_intr_config intr_config;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		default:
			printf("%s: Invalid argument.", getprogname());
			return EXIT_FAILURE;
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (argc == 0) {
		printf("%s: No pin number specified.\n", getprogname());
		return EXIT_FAILURE;
	}

	errno = 0;
	intr_config.gp_pin = strtol(argv[0], NULL, 10);
	if (errno != 0)
		err(EXIT_FAILURE, "Invalid pin number");


	if (argc == 1) {
		printf("%s: No trigger type specified.\n", getprogname());
		return EXIT_FAILURE;
	}

	if (strnlen(argv[1], 2) < 2) {
		printf("%s: Invalid trigger type (argument too short).\n", getprogname());
		return EXIT_FAILURE;
	}

	switch((argv[1][0] << 8) + argv[1][1]) {
		case ('n' << 8) + 'o':
			intr_config.gp_intr_flags = GPIO_INTR_NONE;
			break;
		case ('l' << 8) + 'l':
			intr_config.gp_intr_flags = GPIO_INTR_LEVEL_LOW;
			break;
		case ('l' << 8) + 'h':
			intr_config.gp_intr_flags = GPIO_INTR_LEVEL_HIGH;
			break;
		case ('e' << 8) + 'r':
			intr_config.gp_intr_flags = GPIO_INTR_EDGE_RISING;
			break;
		case ('e' << 8) + 'f':
			intr_config.gp_intr_flags = GPIO_INTR_EDGE_FALLING;
			break;
		case ('e' << 8) + 'b':
			intr_config.gp_intr_flags = GPIO_INTR_EDGE_BOTH;
			break;
		default:
			printf("%s: Invalid trigger type.\n", getprogname());
			return EXIT_FAILURE;
	}

	fd = open(file, O_RDONLY);
	if(fd == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);

	res = ioctl(fd, GPIOINTRCONFIG, &intr_config);
	if(res < 0)
		err(EXIT_FAILURE, "ioctl() on %s failed", file);

	printf("%s: ioctl() on %s successful\n", getprogname(), file);
	return EXIT_SUCCESS;
}
