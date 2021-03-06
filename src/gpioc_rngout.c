#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <libgpio.h>

#include "pcg_variants.h"

#include "rngtestparam.h"

void
usage()
{
	fprintf(stderr, "usage: %s [-f gpioctldev] pin\n", getprogname());
}

int
main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpioc0";
	int errno;
	ssize_t res;
	pcg32_random_t rng;
	uint32_t rand_buffer;
	gpio_handle_t handle;
	gpio_config_t pin_config;
	gpio_value_t value;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
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
		fprintf(stderr, "%s: No pin number specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	errno = 0;
	pin_config.g_pin = strtol(argv[0], NULL, 10);
	if (errno != 0) {
		warn("Invalid pin number");
		usage();
		return EXIT_FAILURE;
	}

	handle = gpio_open_device(file);
	if (handle == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);

	pin_config.g_flags = GPIO_PIN_OUTPUT;

	errno = gpio_pin_set_flags(handle, &pin_config);
	if (errno < 0)
		err(EXIT_FAILURE, "configuration of pin %d on %s failed "
		    "(flags=%d)", pin_config.g_pin, file, pin_config.g_flags);

	pcg32_srandom_r(&rng, SEED1, SEED2);

	for (;;) {

		rand_buffer = pcg32_random_r(&rng);

		for(int i = 0; i <= 31; i++) {
			value = (rand_buffer & 1) ? GPIO_VALUE_LOW :
			    GPIO_VALUE_HIGH;
			rand_buffer >>= 1;
			gpio_pin_set(handle, pin_config.g_pin, value);
			printf("%s: pin %d on %s -> %d\n", getprogname(),
			    pin_config.g_pin, file, value);
			usleep(DELAY);
		}

	}

	return EXIT_SUCCESS;
}
