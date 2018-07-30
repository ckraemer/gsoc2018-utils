#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <libgpio.h>

void
usage()
{
	fprintf(stderr, "usage: %s [-f ctldev] [-a] [pin intr-config ...]\n\n",
	    getprogname());
	fprintf(stderr, "Possible options for intr-config:\n\n");
	fprintf(stderr, "no\tno interrupt\n");
	fprintf(stderr, "ll\t level low\n");
	fprintf(stderr, "lh\t level high\n");
	fprintf(stderr, "er\t edge rising\n");
	fprintf(stderr, "ef\t edge falling\n");
	fprintf(stderr, "eb\t edge both\n");
}

static const char*
flag_to_str(uint32_t flag)
{
	switch (flag) {
	case GPIO_PIN_INPUT:
		return "GPIO_PIN_INPUT";
	case GPIO_PIN_OUTPUT:
		return "GPIO_PIN_OUTPUT";
	case GPIO_PIN_OPENDRAIN:
		return "GPIO_PIN_OPENDRAIN";
	case GPIO_PIN_PUSHPULL:
		return "GPIO_PIN_PUSHPULL";
	case GPIO_PIN_TRISTATE:
		return "GPIO_PIN_TRISTATE";
	case GPIO_PIN_PULLUP:
		return "GPIO_PIN_PULLUP";
	case GPIO_PIN_PULLDOWN:
		return "GPIO_PIN_PULLDOWN";
	case GPIO_PIN_INVIN:
		return "GPIO_PIN_INVIN";
	case GPIO_PIN_INVOUT:
		return "GPIO_PIN_INVOUT";
	case GPIO_PIN_PULSATE:
		return "GPIO_PIN_PULSATE";
	case GPIO_PIN_PRESET_LOW:
		return "GPIO_PIN_PRESET_LOW";
	case GPIO_PIN_PRESET_HIGH:
		return "GPIO_PIN_PRESET_HIGH";
	case GPIO_INTR_LEVEL_LOW:
		return "GPIO_INTR_LEVEL_LOW";
	case GPIO_INTR_LEVEL_HIGH:
		return "GPIO_INTR_LEVEL_HIGH";
	case GPIO_INTR_EDGE_RISING:
		return "GPIO_INTR_EDGE_RISING";
	case GPIO_INTR_EDGE_FALLING:
		return "GPIO_INTR_EDGE_FALLING";
	case GPIO_INTR_EDGE_BOTH:
		return "GPIO_INTR_EDGE_BOTH";
	case GPIO_INTR_ATTACHED:
		return "GPIO_INTR_ATTACHED";
	default:
		return "UNKNOWN FLAG";
	}
}

void
print_pin_config(gpio_config_t *pin_config)
{
	uint32_t curr_flag = 0;
	bool first = true;

	printf("pin %*d:\t", 2, pin_config->g_pin);
	for (int i = 0; i <= sizeof(uint32_t) * CHAR_BIT - 1; i++) {
		curr_flag = 1 << i;
		if ((curr_flag & pin_config->g_flags) == 0)
			continue;
		if (!first) {
			printf(" | ");
		} else {
			first = false;
		}
		printf("%s", flag_to_str(curr_flag));
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpioc0";
	bool all = false;
	int handle;
	int res;
	gpio_config_t pin_config;

	while ((ch = getopt(argc, argv, "af:")) != -1) {
		switch (ch) {
		case 'a':
			all = true;
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

	if (argc % 2 == 1) {
		fprintf(stderr, "%s: Invalid number of pin intr-conf pairs.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	handle = gpio_open_device(file);
	if (handle == GPIO_INVALID_HANDLE)
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
			fprintf(stderr, "%s: Invalid trigger type (argument "
			    "too short).\n", getprogname());
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
			fprintf(stderr, "%s: Invalid trigger type.\n",
			    getprogname());
			usage();
			return EXIT_FAILURE;
		}

		pin_config.g_flags |= GPIO_PIN_INPUT | GPIO_PIN_PULLUP;

		res = gpio_pin_set_flags(handle, &pin_config);
		if (res < 0)
			err(EXIT_FAILURE, "configuration of pin %d on %s "
			    "failed (flags=%d)", pin_config.g_pin, file,
			    pin_config.g_flags);

		if (all == false) {
			res = gpio_pin_config(handle, &pin_config);
			if (res < 0)
				err(EXIT_FAILURE, "retrieving configuration of "
				    "pin %d on %s failed", pin_config.g_pin,
				    file);
			print_pin_config(&pin_config);
		}
	}

	if (all == true) {
	        int n;
		gpio_config_t *pin_configs;
		n = gpio_pin_list(handle, &pin_configs);
		if (n < 0)
			err(EXIT_FAILURE, "retrieving configuration of all pins"
			    "on %s failed", file);
		for (int i = 0; i <= n; i++)
			print_pin_config(&pin_configs[i]);
	}

	return EXIT_SUCCESS;
}
