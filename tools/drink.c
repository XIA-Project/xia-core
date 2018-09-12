/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include "Xsocket.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>

#define VERSION "v1.1"
#define TITLE "XIA Firehose Drinker"
#define NAME "firehose.xia"
#define PKTSIZE 4096

typedef struct {
	uint32_t pktSize;
	uint32_t numPkts;
	uint32_t delay;
} fhConfig;

char *name;
fhConfig fhc;
int verbose = 0;
int pktSize = PKTSIZE;
int numPkts = 0;
int delay = 0;
int sock = -1;
int timetodie = 0;

struct sigaction sa_new;
struct sigaction sa_term;
struct sigaction sa_int;


void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s -v [-d <usec>][-r <usec>][-p <n>][-s <s>]\n", name);
	printf("where:\n");
	printf(" -d <usec>: delay for usecs between each receive\n");
	printf("            default is no delay\n");
	printf(" -r <usec>: tell firehose to wait for usecs between sends\n");
	printf("            default is no delay\n");
	printf(" -p <n>   : tell firehose to send n packets\n");
	printf("            default is nonstop\n");
	printf(" -s <n>   : tell firehose to send packets of size n\n");
	printf("            default is 4096\n");
	printf(" -v       : be verbose\n");
	exit(-1);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;
	opterr = 0;

	fhc.delay = 0;
	fhc.numPkts = 0;
	fhc.pktSize = PKTSIZE;

	while ((c = getopt(argc, argv, "d:p:r:s:v")) != -1) {
		switch (c) {
			case 'p':
				fhc.numPkts = atoi(optarg);
				numPkts = fhc.numPkts;
				break;
			case 'd':
				delay = atoi(optarg);
				break;
			case 'r':
				fhc.delay = atoi(optarg);
				break;
			case 's':
				fhc.pktSize = atoi(optarg);
				pktSize = fhc.pktSize;
				break;
			case 'v':
				verbose = 1;
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
		}
	}

	if (optind != argc)
		name = strdup(argv[optind]);
	else
		name = strdup(NAME);

	fhc.delay = htonl(fhc.delay);
	fhc.numPkts = htonl(fhc.numPkts);
	fhc.pktSize = MIN(htonl(fhc.pktSize), XmaxPayload());
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


int main(int argc, char **argv)
{
	struct addrinfo *ai;
	sockaddr_x *sa;
	int seq = 0;

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

	getConfig(argc, argv);

	if (Xgetaddrinfo(NAME, NULL, NULL, &ai) < 0)
		die("Unable to lookup address for  %s\n", NAME);
	sa = (sockaddr_x*)ai->ai_addr;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die("Unable to create a socket\n");

	say("Opening firehose: %s\n", NAME);
	if (Xconnect(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		die("Unable to connect to %s\n", NAME);
	}

	// tell firehose how many packets we expect
	if (Xsend(sock, &fhc, sizeof(fhc), 0) < 0) {
		Xclose(sock);
		die("Unable to send config information to the firehose\n");
	}

	int count = 0;
	int lost = 0;
	char  *buf = (char *)malloc(pktSize);

	while (!timetodie) {
		int received = 0;
		char *bp = buf;
		int sz = ntohl(fhc.pktSize);

		while (received < sz) {
			int rc = Xrecv(sock, bp, sz - received, 0);
			if (rc < 0)
				die("Receive failure\n");
			else if(rc == 0) {
				printf("Peer closed the connection\n");
				goto done;
			}
			received += rc;
			bp += rc;
		}
		memcpy(&seq, buf, sizeof(int));


		say("expecting %d, got %d", count, seq);
		if (count == seq) {
			say("\n");
}
		else {
			say(" lost packet\n");
			count = seq;
			lost += seq - count;
		}
		count++;
		if (count == numPkts)
			break;
		if (delay)
			usleep(delay);
	}
done:
	seq++;
	if (count != seq)
		printf("lost %d packets, received %d, expected %d\n", lost, count, seq);
	else
		printf("success! received %d packets\n", count);
	Xclose(sock);
}
