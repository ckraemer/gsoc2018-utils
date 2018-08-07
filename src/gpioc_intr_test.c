#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <aio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <err.h>

#include <sys/poll.h>
#include <sys/select.h>
#include <sys/event.h>

#include <libgpio.h>

static volatile sig_atomic_t sigio = 0;

static void
sigio_handler(int sig){
	sigio = 1;
}

void
usage()
{
	fprintf(stderr, "usage: %s [-f ctldev] [-m method] [-s] [-n] "
	    "[-t timeout] pin intr-config [pin intr-config ...]\n\n",
	    getprogname());
	fprintf(stderr, "Possible options for method:\n\n");
	fprintf(stderr, "r\tread (default)\n");
	fprintf(stderr, "p\tpoll\n");
	fprintf(stderr, "s\tselect\n");
	fprintf(stderr, "k\tkqueue\n");
	fprintf(stderr, "a\taio_read\n");
	fprintf(stderr, "i\tsignal-driven I/O\n\n");
	fprintf(stderr, "Possible options for intr-config:\n\n");
	fprintf(stderr, "no\tno interrupt\n");
	fprintf(stderr, "ll\t level low\n");
	fprintf(stderr, "lh\t level high\n");
	fprintf(stderr, "er\t edge rising\n");
	fprintf(stderr, "ef\t edge falling\n");
	fprintf(stderr, "eb\t edge both\n");
}

static const char*
intr_mode_to_str(uint32_t intr_mode)
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

static const char*
poll_event_to_str(short event)
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

void
print_events(short event)
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

uint32_t
buffer_to_pin(char *buffer)
{
	/* Assuming little-endian */
	return ((uint32_t)buffer[3] << 24 | (uint32_t)buffer[2] << 16 |
	        (uint32_t)buffer[1] <<  8 | (uint32_t)buffer[0]);
}

void
run_read(bool loop, int handle, char *file)
{
	ssize_t res;
	char buffer[4];
	uint32_t pin;

	do {
		res = read(handle, buffer, sizeof(buffer));
		if (res < 0)
			err(EXIT_FAILURE, "Cannot read from %s", file);

		if (res == sizeof(pin)) {
			pin = buffer_to_pin(buffer);
			printf("%s: read() interrupt on pin %d of %s\n",
			    getprogname(), pin, file);
		} else {
			printf("%s: read() %i bytes from %s\n", getprogname(),
			    res, file);
		}
	} while (loop);
}

void
run_poll(bool loop, int handle, char *file, int timeout)
{
	struct pollfd fds;
	int res;

        fds.fd = handle;
        fds.events = POLLIN | POLLRDNORM;
        fds.revents = 0;

	do {
		res = poll(&fds, 1, timeout);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot poll() %s", file);
		} else if (res == 0) {
			printf("%s: poll() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: poll() returned %i (revents: ",
			    getprogname(), res);
			print_events(fds.revents);
			printf(") on %s\n", file);
			if (fds.revents & (POLLHUP | POLLERR)) {
				err(EXIT_FAILURE, "Recieved POLLHUP or POLLERR "
				    "on %s", file);
			}
			run_read(false, handle, file);
		}
	} while (loop);
}

void
run_select(bool loop, int handle, char *file, int timeout)
{
	fd_set readfds;
	struct timeval tv;
	struct timeval *tv_ptr;
	int res;

	FD_ZERO(&readfds);
	FD_SET(handle, &readfds);
	if (timeout != INFTIM) {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tv_ptr = &tv;
	} else {
		tv_ptr = NULL;
	}

	do {
		res = select(FD_SETSIZE, &readfds, NULL, NULL, tv_ptr);
		if (res < 0) {
			err(EXIT_FAILURE, "Cannot select() %s", file);
		} else if (res == 0) {
			printf("%s: select() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: select() returned %i on %s\n",
			    getprogname(), res, file);
			run_read(false, handle, file);
		}
	} while (loop);
}

void
run_kqueue(bool loop, int handle, char *file, int timeout)
{
	struct kevent event[1];
	struct kevent tevent[1];
	int kq = -1;
	int nev = -1;
	struct timespec tv;
	struct timespec *tv_ptr;
	int res;

	if (timeout != INFTIM) {
		tv.tv_sec = timeout / 1000;
		tv.tv_nsec = (timeout % 1000) * 10000000;
		tv_ptr = &tv;
	} else {
		tv_ptr = NULL;
	}

	kq = kqueue();
	if (kq == -1)
		err(EXIT_FAILURE, "kqueue() %s", file);

	EV_SET(&event[0], handle, EVFILT_READ, EV_ADD, 0, 0, NULL);
	nev = kevent(kq, event, 1, NULL, 0, NULL);
	if (nev == -1)
		err(EXIT_FAILURE, "kevent() %s", file);

	do {
		nev = kevent(kq, NULL, 0, tevent, 1, tv_ptr);
		if (nev == -1) {
			err(EXIT_FAILURE, "kevent() %s", file);
		} else if (nev == 0) {
			printf("%s: kevent() timed out on %s\n", getprogname(),
			    file);
		} else {
			printf("%s: kevent() returned %i events (flags: %d) on "
			    "%s\n", getprogname(), nev, tevent[0].flags, file);
			if (tevent[0].flags & EV_EOF) {
				err(EXIT_FAILURE, "Recieved EV_EOF on %s",
				    file);
			}
			run_read(false, handle, file);
		}
	} while (loop);
}

void
run_aio_read(bool loop, int handle, char *file)
{
	int res;
	char buffer[4];
	struct aiocb iocb;
	uint32_t pin;

	bzero(&iocb, sizeof(iocb));

	iocb.aio_fildes = handle;
	iocb.aio_nbytes = sizeof(buffer);
	iocb.aio_offset = 0;
	iocb.aio_buf = buffer;

	do {
		res = aio_read(&iocb);
		if (res < 0)
		err(EXIT_FAILURE, "Cannot aio_read from %s", file);
		do {
			res = aio_error(&iocb);
		} while (res == EINPROGRESS);
		if (res < 0)
			err(EXIT_FAILURE, "aio_error on %s", file);
		res = aio_return(&iocb);
		if (res < 0)
			err(EXIT_FAILURE, "aio_return on %s", file);
		if (res == sizeof(pin)) {
			pin = buffer_to_pin(buffer);
			printf("%s: aio_read() interrupt on pin %d of %s\n",
			    getprogname(), pin, file);
		} else {
			printf("%s: aio_read() read %i bytes from %s\n",
			    getprogname(), res, file);
		}
	} while (loop);
}


void
run_sigio(bool loop, int handle, char *file)
{
	int res;
	struct sigaction sigact;
	int flags;
	int pid;

	bzero(&sigact, sizeof(sigact));
	sigact.sa_handler = sigio_handler;
	if (sigaction(SIGIO, &sigact, NULL) < 0)
		err(EXIT_FAILURE, "cannot set SIGIO handler on %s", file);
	flags = fcntl(handle, F_GETFL);
	flags |= O_ASYNC;
	res = fcntl(handle, F_SETFL, flags);
	if (res < 0)
		err(EXIT_FAILURE, "fcntl(F_SETFL) on %s", file);
	pid = getpid();
	res = fcntl(handle, F_SETOWN, pid);
	if (res < 0)
		err(EXIT_FAILURE, "fnctl(F_SETOWN) on %s", file);

	do {
		if (sigio == 1) {
			sigio = 0;
			printf("%s: recieved SIGIO on %s\n", getprogname(),
			    file);
			run_read(false, handle, file);
		}
		pause();
	} while (loop);
}

int
main(int argc, char *argv[])
{
	int ch;
	char *file = "/dev/gpioc0";
	char method = 'r';
	bool loop = true;
	bool nonblock = false;
	int flags;
	int timeout = INFTIM;
	int handle;
	int res;
	gpio_config_t pin_config;

	while ((ch = getopt(argc, argv, "f:m:snt:")) != -1) {
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
		case 'n':
			nonblock= true;
			break;
		case 't':
			errno = 0;
			timeout = strtol(optarg, NULL, 10);
			if (errno != 0) {
				warn("Invalid timeout value");
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

	if (argc == 0) {
		fprintf(stderr, "%s: No pin number specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		fprintf(stderr, "%s: No trigger type specified.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	if (argc % 2 == 1) {
		fprintf(stderr, "%s: Invalid number of pin intr-conf pairs.\n",
		    getprogname());
		usage();
		return EXIT_FAILURE;
	}

	handle = gpio_open_device(file);
	if (handle == GPIO_INVALID_HANDLE)
		err(EXIT_FAILURE, "Cannot open %s", file);

	if (nonblock == true) {
		flags = fcntl(handle, F_GETFL);
		flags |= O_NONBLOCK;
		res = fcntl(handle, F_SETFL, flags);
		if (res < 0)
			err(EXIT_FAILURE, "cannot set O_NONBLOCK on %s", file);
	}

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
	}

	switch (method) {
	case 'r':
		run_read(loop, handle, file);
		break;
	case 'p':
		run_poll(loop, handle, file, timeout);
		break;
	case 's':
		run_select(loop, handle, file, timeout);
		break;
	case 'k':
		run_kqueue(loop, handle, file, timeout);
		break;
	case 'a':
		run_aio_read(loop, handle, file);
		break;
	case 'i':
		run_sigio(loop, handle, file);
		break;
	default:
		fprintf(stderr, "%s: Unknown method.\n", getprogname());
		usage();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
