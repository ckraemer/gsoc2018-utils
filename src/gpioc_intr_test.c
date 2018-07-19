#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <libgpio.h>

void usage()
{
	fprintf(stderr, "usage: %s [-f ctldev] [-m method] [-s] pin intr-config [pin intr-config ...]\n\n", getprogname());
	fprintf(stderr, "Possible options for method:\n\n");
	fprintf(stderr, "r\tread (default)\n\n");
	fprintf(stderr, "Possible options for intr-config:\n\n");
	fprintf(stderr, "no\tno interrupt\n");
	fprintf(stderr, "ll\t level low\n");
	fprintf(stderr, "lh\t level high\n");
	fprintf(stderr, "er\t edge rising\n");
	fprintf(stderr, "ef\t edge falling\n");
	fprintf(stderr, "eb\t edge both\n");
}

static const char* intr_mode_to_str(uint32_t intr_mode)
{
	switch (intr_mode) {
	case GPIO_INTR_LEVEL_LOW:
		return "low level";
	case GPIO_INTR_LEVEL_HIGH:
		return "high level";
	case GPIO_INTR_EDGE_RISING:
		return "rising edge";
	case GPIO_INTR_EDGE_FALLING:
		return "falling edge";
	case GPIO_INTR_EDGE_BOTH:
		return "both edges";
	default:
		return "invalid mode";
	}
}

void run_read(bool loop, int handle, char *file, char *buffer)
{
	ssize_t res;

	do {
		res = read(handle, buffer, sizeof(buffer));
		if(res < 0)
			err(EXIT_FAILURE, "Cannot read from %s", file);

		printf("%s: Read %i bytes from %s\n", getprogname(), res, file);
	} while (loop);
}

int main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpioc0";
	char method = 'r';
	bool loop = true;
	int handle;
	char buffer[1024];
	int res;
	gpio_config_t pin_config;

	while ((ch = getopt(argc, argv, "f:m:s")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 'm':
			method = optarg[0];
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

	if (argc == 0) {
		fprintf(stderr, "%s: No pin number specified.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		fprintf(stderr, "%s: No trigger type specified.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc % 2 == 1) {
		fprintf(stderr, "%s: Invalid number of pin intr-conf pairs.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	handle = gpio_open_device(file);
	if(handle == GPIO_INVALID_HANDLE)
		err(EXIT_FAILURE, "Cannot open %s", file);

	for (int i = 0; i <= argc - 2; i += 2) {

		errno = 0;
		pin_config.g_pin = strtol(argv[i], NULL, 10);
		if (errno != 0) {
			warn("Invalid pin number");
			usage();
			return EXIT_FAILURE;
		}

		if (strnlen(argv[i + 1], 2) < 2) {
			fprintf(stderr, "%s: Invalid trigger type (argument too short).\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		switch((argv[i + 1][0] << 8) + argv[i + 1][1]) {
		case ('n' << 8) + 'o':
			pin_config.g_flags = GPIO_INTR_NONE;
			break;
		case ('l' << 8) + 'l':
			pin_config.g_flags = GPIO_INTR_LEVEL_LOW;
			break;
		case ('l' << 8) + 'h':
			pin_config.g_flags = GPIO_INTR_LEVEL_HIGH;
			break;
		case ('e' << 8) + 'r':
			pin_config.g_flags = GPIO_INTR_EDGE_RISING;
			break;
		case ('e' << 8) + 'f':
			pin_config.g_flags = GPIO_INTR_EDGE_FALLING;
			break;
		case ('e' << 8) + 'b':
			pin_config.g_flags = GPIO_INTR_EDGE_BOTH;
			break;
		default:
			fprintf(stderr, "%s: Invalid trigger type.\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		pin_config.g_flags |= GPIO_PIN_INPUT | GPIO_PIN_PULLUP;

		res = gpio_pin_set_flags(handle, &pin_config);
		if(res < 0)
			err(EXIT_FAILURE, "configuration of pin %d on %s failed (flags=%d)", pin_config.g_pin, file, pin_config.g_flags);

	}

	switch (method) {
	case 'r':
		run_read(loop, handle, file, buffer);
	default:
		fprintf(stderr, "%s: Unknown method.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
