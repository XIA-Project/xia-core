#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include "Xsocket.h"
#include "Xkeys.h"

#define VERSION "v1.0"
#define TITLE "XIA Firehose"
#define NAME "firehose.xia"
#define PKTSIZE 4096

typedef struct {
	uint32_t pktSize;
	uint32_t numPkts;
	uint32_t delay;
} fhConfig;

int timetodie = 0;

char *name;
int verbose = 0;

void handler(int)
{
	timetodie = 1;
}

char *data(int seq, char *buf, int size)
{
	int i;
	static int refresh = 1;
	int total = size = sizeof(seq) - 1;

	if (!(--refresh)) {
		// refresh rand every now and then so it doesn't degenerate too much
		//  use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}

	// put the sequence at the front of the buffer
	*(int*)buf = seq;

	// fill the remaining buffer with random data
	for (i = sizeof(seq); i < total; i ++) {
		buf[i] = (char)(rand() % 256);
	}
	buf[size - 1] = 0;

	return buf;
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
	int n;

	signal(SIGINT, handler);

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(peer, &fds);

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	n = Xselect(peer + 1, &fds, NULL, NULL, &tv);
	if (n < 0) {
		printf("select failed\n");
		goto done;
	
	} else if (n == 0) {
		printf("recv timeout\n");
		goto done;
	
	} else if (!FD_ISSET(peer, &fds)) {
		// this shouldn't happen!
		printf("something is really wrong, exiting\n");
		goto done;
	}

	if (Xrecv(peer, &fhc, sizeof(fhc), 0) < 0) {
		printf("Unable to get configuration block\n");
		goto done;
	}

	fhc.numPkts = ntohl(fhc.numPkts);
	fhc.delay   = ntohl(fhc.delay);
	fhc.pktSize = ntohl(fhc.pktSize);
	// need to have at least enough room for the sequence #
	fhc.pktSize = MAX(fhc.pktSize, sizeof(unsigned));
	if (fhc.numPkts == 0)
		say("packet count = non-stop\n");
	else
		say("packet count = %d\n", fhc.numPkts);
	say("packet size = %d\n", fhc.pktSize);
	say("inter-packet delay = %d\n", fhc.delay);


	if (!(buf = (char *)malloc(fhc.pktSize))) {
		printf("Memory error\n");
		goto done;
	}
		
	while (!timetodie) {
		if (fhc.numPkts > 0 && count == fhc.numPkts)
			break;
		printf("sending packet %d\n", count);
		data(count, buf, sizeof(buf));
		Xsend(peer, buf, sizeof(buf), 0);
		count++;
		if (fhc.delay != 0)
			usleep(fhc.delay);
	}
done:
	say("done\n");
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

// FIXME: put signal handlers back into code once Xselect is working
//	signal(SIGINT, handler);
//	signal(SIGTERM, handler);

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

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if ((rc = Xselect(sock + 1, &fds, NULL, NULL, &tv)) < 0) {
			printf("select failed\n");
			break;
	
		} else if (rc == 0) {
			// timed out, try again
			continue;
	
		} else if (!FD_ISSET(sock, &fds)) {
			// this shouldn't happen!
			printf("something is really wrong, exiting\n");
			break;
		}

		peer = Xaccept(sock, NULL, NULL);
		if (peer < 0) {
			printf("Xaccept failed\n");
			break;
		}

		say("peer connected...\n");
		pid_t pid = fork();

		if (pid == -1) { 
			printf("fork failed\n");
			break;
		}
		else if (pid == 0) {
			process(peer);
			exit(0);
		}
		else {
			// use regular close so we don't rip out the Xsocket state from under the child
			close(peer);
		}
	}

	say("firehose exiting\n");
	Xclose(sock);
	return 0;
}
