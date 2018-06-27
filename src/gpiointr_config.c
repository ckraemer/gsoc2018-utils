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

#include <libgpio.h>

void usage()
{
	fprintf(stderr, "Usage: %s [-f gpiointrdev] pin intr-config\n", getprogname());
	fprintf(stderr, "       %*c [-f gpiointrdev] -g pin\n\n", strlen(getprogname()), ' ');
	fprintf(stderr, "Possible options for intr-config:\n\n");
	fprintf(stderr, "no\tno interrupt\n");
	fprintf(stderr, "ll\t level low\n");
	fprintf(stderr, "lh\t level high\n");
	fprintf(stderr, "er\t edge rising\n");
	fprintf(stderr, "ef\t edge falling\n");
	fprintf(stderr, "eb\t edge both\n");
}

static const char*
gpiointr_intr_mode_to_str(uint32_t intr_mode)
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

int main(int argc, char *argv[])
{
	int ch;
	bool set_config = true;
	char *file = "/dev/gpiointr0";
	int handle;
	int res;
	gpio_intr_config_t intr_config;

	while ((ch = getopt(argc, argv, "gf:")) != -1) {
		switch (ch) {
		case 'g':
			set_config = false;
			break;
		case 'f':
			file = optarg;
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

	errno = 0;
	intr_config.g_pin = strtol(argv[0], NULL, 10);
	if (errno != 0) {
		warn("Invalid pin number");
		usage();
		return EXIT_FAILURE;
	}

	if (set_config) {

		if (argc == 1) {
			fprintf(stderr, "%s: No trigger type specified.\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		if (strnlen(argv[1], 2) < 2) {
			fprintf(stderr, "%s: Invalid trigger type (argument too short).\n", getprogname());
			usage();
			return EXIT_FAILURE;
		}

		switch((argv[1][0] << 8) + argv[1][1]) {
			case ('n' << 8) + 'o':
				intr_config.g_intr_flags = GPIO_INTR_NONE;
				break;
			case ('l' << 8) + 'l':
				intr_config.g_intr_flags = GPIO_INTR_LEVEL_LOW;
				break;
			case ('l' << 8) + 'h':
				intr_config.g_intr_flags = GPIO_INTR_LEVEL_HIGH;
				break;
			case ('e' << 8) + 'r':
				intr_config.g_intr_flags = GPIO_INTR_EDGE_RISING;
				break;
			case ('e' << 8) + 'f':
				intr_config.g_intr_flags = GPIO_INTR_EDGE_FALLING;
				break;
			case ('e' << 8) + 'b':
				intr_config.g_intr_flags = GPIO_INTR_EDGE_BOTH;
				break;
			default:
				fprintf(stderr, "%s: Invalid trigger type.\n", getprogname());
				usage();
				return EXIT_FAILURE;
		}

	}

	handle = gpio_intr_open_device(file);
	if(handle == GPIO_INVALID_HANDLE)
		err(EXIT_FAILURE, "Cannot open %s", file);

	if (set_config) {

		res = gpio_intr_pin_set_flags(handle, &intr_config);
		if(res < 0)
			err(EXIT_FAILURE, "configuration of pin %d on %s failed", intr_config.g_pin, file);

		printf("%s: pin %d on %s successfully configured\n", getprogname(), intr_config.g_pin, file);
		return EXIT_SUCCESS;

	} else {
		res = gpio_intr_pin_config(handle, &intr_config);
		if(res < 0)
			err(EXIT_FAILURE, "retrieving configuration of pin %d on %s failed", intr_config.g_pin, file);

		printf("%s: pin %d of %s is configured on %s\n", getprogname(), intr_config.g_pin, file, gpiointr_intr_mode_to_str(intr_config.g_intr_flags));
		return EXIT_SUCCESS;
	}

}
