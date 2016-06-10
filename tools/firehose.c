#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include "Xsocket.h"
#include "Xkeys.h"

#define VERSION "v1.1"
#define TITLE "XIA Firehose"
#define NAME "firehose.xia"
#define PKTSIZE 4096

typedef struct {
	uint32_t pktSize;
	uint32_t numPkts;
	uint32_t delay;
} fhConfig;

int timetodie = 0;

struct sigaction sa_new;
struct sigaction sa_term;
struct sigaction sa_int;

char *name;
int verbose = 0;

void handler(int sig)
{
	timetodie = 1;

	// chain to the old handler if it exists
	switch (sig) {
		case SIGTERM:
			if (sa_term.sa_handler)
				(sa_term.sa_handler)(sig);
			break;
		case SIGINT:
			if (sa_int.sa_handler)
				(sa_int.sa_handler)(sig);
			break;
		default:
			break;
	}
}

void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

void die(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(-1);
}

void process(int peer)
{
	fhConfig fhc;
	char *buf = NULL;
	uint32_t count = 0;
	int rc;

	if (Xrecv(peer, &fhc, sizeof(fhc), 0) < 0) {
		printf("Unable to get configuration block\n");
		goto done;
	}

	fhc.numPkts = ntohl(fhc.numPkts);
	fhc.delay   = ntohl(fhc.delay);
	fhc.pktSize = ntohl(fhc.pktSize);
	// need to have at least enough room for the sequence #
	fhc.pktSize = MAX(fhc.pktSize, sizeof(uint32_t));
	if (fhc.numPkts == 0)
		say("packet count = non-stop\n");
	else
		say("packet count = %d\n", fhc.numPkts);
	say("packet size = %d\n", fhc.pktSize);
	say("inter-packet delay = %d\n", fhc.delay);

	if (!(buf = (char *)calloc(fhc.pktSize, 1))) {
		printf("Memory error\n");
		goto done;
	}

	while (!timetodie) {
		if (fhc.numPkts > 0 && count == fhc.numPkts)
			break;

		// stick the count value at the front of the packet so we can tell if any get Lost
		// the rest of the packet will be 0's
		*(uint32_t*)buf = count;

		if ((rc = Xsend(peer, buf, fhc.pktSize, 0)) < 0) {
			Xclose(peer);
			if (timetodie) {
				die("session terminated\n");
			} else {
				die("Lost connection to the client\n");
			}
		} else if (rc == 0) {
			Xclose(peer);
			die("xsend returned 0, this shouldn't happen!\n");
		}

		count++;
		if (fhc.delay != 0) {
			usleep(fhc.delay);
		}
	}
done:
	say("done: sent %u packets\n", count);
	if (buf)
		free(buf);
	Xclose(peer);
}

void help()
{
	printf("usage: firehose [-v] name\n");
	printf("where:\n");
	printf(" -v : run in verbose mode\n");
	printf("name = XIA name to listen for connections on\n");
	printf("default is %s\n", NAME);
	exit(-1);
}

void configure(int argc, char** argv)
{
	int c;
	opterr = 0;

	while ((c = getopt(argc, argv, "hv")) != -1) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 'h':
			case '?':
			default:
				help();
		}
	}

	if (optind != argc)
		name = strdup(argv[optind]);
	else
		name = strdup(NAME);
}

int main(int argc, char **argv)
{
	int sock = -1;
	int peer = -1;
	struct addrinfo hints, *ai;
	char sid[50];
	pid_t pid;

	memset (&sa_new, 0, sizeof (struct sigaction));
	sigemptyset (&sa_new.sa_mask);
	sa_new.sa_handler = handler;
	sa_new.sa_flags = 0;

	sigaction (SIGINT, NULL, &sa_int);
	if (sa_int.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &sa_new, &sa_int);
	}
	sigaction (SIGTERM, NULL, &sa_term);
	if (sa_int.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &sa_new, &sa_term);
	}

	configure(argc, argv);
	say("XIA firehose listening on %s\n", name);

	// Generate an SID to use
	if(XmakeNewSID(sid, sizeof(sid))) {
		die("Unable to create a temporary SID");
	}
	// get our local AD/HID and append the SID to the resultant dag
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV;
	int rc = Xgetaddrinfo(NULL, sid, &hints, &ai);
	if (rc != 0)
		die("%s\n", Xgai_strerror(rc));

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;
	if ( XregisterName(NAME, sa) < 0)
		die("Unable to register name %s\n", name);

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die("Unable to create socket\n");
	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		die("Unable to bind to DAG\n");
	}

	if (Xlisten(sock, 5) < 0) {
		Xclose(sock);
		die("Listen failed\n");
	}

	while (!timetodie) {
		peer = Xaccept(sock, NULL, NULL);
		if (peer < 0) {
			printf("Xaccept failed\n");
			break;
		}

		say("drinker connected...\n");
		pid = Xfork();

		if (pid == -1) {
			printf("fork failed\n");
			break;
		}
		else if (pid == 0) {
			Xclose(sock);
			process(peer);
			exit(0);
		}
		else {
			Xclose(peer);
		}
	}

	if (pid != 0) {
		say("firehose server exiting\n");
		Xclose(sock);
	}
	return 0;
}
