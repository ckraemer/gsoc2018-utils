#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/time.h>

#include <libgpio.h>

#include "pcg_variants.h"

#include "rngtestparam.h"

void
usage()
{
	fprintf(stderr, "usage: %s [-f ctldev] pin\n", getprogname());
}

struct rng_state {
	bool value;
	int i;
	uint32_t rand_buffer;
};

long
predict_next_timestamp(pcg32_random_t *rng, struct rng_state *rs)
{
	int n = 1;
	bool value = rs->value;

	for (;;) {
		for (; rs->i <= 31; rs->i++) {
			value = (rs->rand_buffer & 1);
			rs->rand_buffer >>= 1;
			if (value != rs->value) {
				rs->i++;
				rs->value = value;
				return (n * DELAY);
			}
			n++;
		}
		rs->i = 0;
		rs->rand_buffer = pcg32_random_r(rng);
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpioc0";
	int errno;
	int handle;
	char buffer[1024];
	ssize_t res;
	gpio_config_t pin_config;
	pcg32_random_t rng;
	struct rng_state rs;
	bool value, value_last;
	struct timeval tp_1;
	struct timeval tp_2;
	struct timeval tp_diff;
	long t_diff;
	long t_pred;

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

	handle = open(file, O_RDONLY);
	if (handle == -1)
		err(EXIT_FAILURE, "Cannot open %s", file);

	pin_config.g_flags = GPIO_PIN_INPUT | GPIO_PIN_PULLUP |
	    GPIO_INTR_EDGE_BOTH;

	res = gpio_pin_set_flags(handle, &pin_config);
	if (res < 0)
		err(EXIT_FAILURE, "configuration of pin %d on %s failed "
		    "(flags=%d)", pin_config.g_pin, file, pin_config.g_flags);

	pcg32_srandom_r(&rng, SEED1, SEED2);

	rs.value = false;
	rs.i = 0;
	rs.rand_buffer = pcg32_random_r(&rng);

	predict_next_timestamp(&rng, &rs);

	for (;;) {

		errno = gettimeofday(&tp_1, NULL);
		if (errno < 0)
			err(EXIT_FAILURE, "Cannot get time");

		t_pred = predict_next_timestamp(&rng, &rs);

		res = read(handle, buffer, sizeof(buffer));
		if(res < 0)
			err(EXIT_FAILURE, "Cannot read from %s", file);

		errno = gettimeofday(&tp_2, NULL);
		if (errno < 0)
			err(EXIT_FAILURE, "Cannot get time");

		timersub(&tp_2, &tp_1, &tp_diff);

		/* ToDo: Check for overflow */
		t_diff = tp_diff.tv_sec * 1000000 + tp_diff.tv_usec;

		printf("%s: event on %s (expected: %ld, arrived: %ld diff: %ld)"
		    "\n", getprogname(), file, t_pred, t_diff, t_diff - t_pred);

	}

	return EXIT_SUCCESS;
}
