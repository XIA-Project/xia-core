#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <iostream>
#include "controller.h"
#include "clicknet/xia.h"
#include <getopt.h>

static char version[] = "1.0";
static char ident[256]; // syslog identifcation string

static void display_version(void)
{
	printf("XIA Cache Daemon version %s\n", version);
}

static void usage(char *argv[])
{
	display_version();
	printf("Usage: %s [OPTIONS]\n", argv[0]);
	printf("  -h, --host=HOSTNAME       Specify the host on which"
		   " this daemon should run.\n");
	printf("                            Default host = \"host0\"\n");
	printf("  -V, --version             Displays xcache version.\n");
	printf("  -l, --log-level=level     Syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf("  -v, --verbose             Log to the console as well as syslog\n");
	printf("  -h, --help                Displays this help.\n");
}

static void sethostname(struct xcache_conf *conf, char *hostname)
{
	strcpy(conf->hostname, hostname);
}

static void initlogging(std::string  hostname, unsigned level, bool verbose)
{
	unsigned flags = LOG_CONS|LOG_NDELAY;

	if (verbose) {
		flags |= LOG_PERROR;
	}
	sprintf(ident, "xcache:%s", hostname.c_str());
	openlog(ident, flags, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));

	syslog(LOG_NOTICE, "xcache started on %s", hostname.c_str());
}


int main(int argc, char *argv[])
{
	int c;
	char *p;
	unsigned level = 3;
	bool verbose = false;

	xcache_controller *ctrl = xcache_controller::get_instance();
	struct xcache_conf xcache_conf;

	xcache_conf.threads = DEFAULT_THREADS;

	gethostname(xcache_conf.hostname, sizeof(xcache_conf.hostname));
	if ((p = strchr(xcache_conf.hostname, '.')) != NULL) {
		*p = '\0';
	}


	struct option options[] = {
		{"host", required_argument, 0, 0},
		{"help", no_argument, 0, 0},
		{"version", no_argument, 0, 0},
		{"level", required_argument, 0, 0},
		{"verbose", required_argument, 0, 0},
		{"threads", required_argument, 0, 0},
		{0, 0, 0, 0},
	};

	while(1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "Vvl:h:t:", options, &option_index);
		if(c == -1)
			break;

		switch(c) {
		case 0:
			/* long option passed */
			if (!strcmp(options[option_index].name, "host")) {
				sethostname(&xcache_conf, optarg);
			} else if (!strcmp(options[option_index].name, "help")) {
				usage(argv);
				return 0;
			} else if (!strcmp(options[option_index].name, "version")) {
				display_version();
				return 0;
			} else if (!strcmp(options[option_index].name, "log_level")) {
				level = MIN(strtol(optarg, NULL, 10), LOG_DEBUG);
				return 0;
			} else if (!strcmp(options[option_index].name, "verbose")) {
				verbose = true;
				break;
			} else if (!strcmp(options[option_index].name, "threads")) {
				xcache_conf.threads = strtol(optarg, NULL, 0);
				break;
			} else {
				usage(argv);
				return 1;
			}
			break;
		case 'h':
			sethostname(&xcache_conf, optarg);
			break;
		case 'l':
			level = MIN(strtol(optarg, NULL, 10), LOG_DEBUG);
			break;
		case 'v':
			verbose = true;
			break;
		case 'V':
			display_version();
			return 0;
		case 't':
			xcache_conf.threads = strtol(optarg, NULL, 0);
			break;
		default:
			usage(argv);
			return 1;
		}
	}
	initlogging(xcache_conf.hostname, level, verbose);

	ctrl->set_conf(&xcache_conf);
	ctrl->run();
	return 0;
}
