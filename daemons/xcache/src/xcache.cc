#include <stdio.h>
#include <iostream>
#include "controller.h"
#include "clicknet/xia.h"
#include <getopt.h>
#include "logger.h"

DEFINE_LOG_MACROS(XCACHE)


static char version[] = "0.1";

static void display_version(void)
{
	printf("XIA Cache Daemon version %s\n", version);
}

static void usage(char *argv[])
{
	int i;
	char mask[][128] = {
		"Controller",
		"Cache",
		"Slice",
		"Meta",
		"Stores",
		"Policies",
		"Xcache",
	};

	display_version();
	printf("Usage: %s [OPTIONS]\n", argv[0]);
	printf("  -h, --host=HOSTNAME       Specify the host on which"
		   " this daemon should run.\n");
	printf("                            Default host = \"host0\"\n");
	printf("  -v, --version             Displays xcache version.\n");
	printf("  -l, --log-level=level     Set xcache log level"
		   "(From LOG_ERROR = %d to LOG_INFO = %d).\n",
		   LOG_ERROR, LOG_INFO);
	printf("  -m, --log-mask=<num>      Set xcache log Mask. "
		   "Following are the mask values for various modules\n");
	for(i = LOG_CTRL; i < LOG_END; i++) {
		printf("                            Mask for module \"%s\": 0x%4.4x\n",
			   mask[i], 0x1 << i);
	}


	printf("                            Default Mask = 0xFFFF "
		   "(Enables all logs)\n");
	printf("  --help                    Displays this help.\n");
}

static void sethostname(struct xcache_conf *conf, char *hostname)
{
	strcpy(conf->hostname, hostname);
}

int main(int argc, char *argv[])
{
	int c;
	xcache_controller ctrl;
	struct xcache_conf xcache_conf;
	struct logger_config logger_conf;

	logger_conf.level = 0;
	logger_conf.mask = LOG_ALL;
	xcache_conf.threads = DEFAULT_THREADS;

	strcpy(xcache_conf.hostname, "router0");

	struct option options[] = {
		{"host", required_argument, 0, 0},
		{"help", no_argument, 0, 0},
		{"version", no_argument, 0, 0},
		{"log_level", required_argument, 0, 0},
		{"log_mask", required_argument, 0, 0},
		{"threads", required_argument, 0, 0},
		{0, 0, 0, 0},
	};

	while(1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "vl:m:h:t:", options, &option_index);
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
				logger_conf.level = strtol(optarg, NULL, 10);
				return 0;
			} else if (!strcmp(options[option_index].name, "log_mask")) {
				logger_conf.mask = strtol(optarg, NULL, 0);
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
			logger_conf.level = strtol(optarg, NULL, 10);
			break;
		case 'm':
			logger_conf.mask = strtol(optarg, NULL, 0);
			break;
		case 'v':
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

	configure_logger(&logger_conf);
	ctrl.set_conf(&xcache_conf);
	ctrl.run();
	return 0;
}

bool operator<(const struct click_xia_xid& x, const struct click_xia_xid& y)
{
	if(x.type < y.type) {
		return true;
	} else if(x.type > y.type) {
		return false;
	}

	for(int i = 0; i < CLICK_XIA_XID_ID_LEN; i++) {
		if(x.id[i] < y.id[i])
			return true;
		else if(x.id[i] > y.id[i])
			return false;
	}

	return false;
}
