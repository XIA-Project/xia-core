/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
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
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Echo Client"

#define DAG  "RE %s %s %s"
#define AD0  "AD:1000000000000000000000000000000000000000"
#define HID0 "HID:0000000000000000000000000000000000000000"
#define SID0 "SID:0f00000000000000000000000000000000000777"


// global configuration options
int verbose = 1;
int delay = -1;
int loops = 1;
int  pktSize = 512;

char *createDAG(const char *ad, const char *host, const char *service)
{
	int len = snprintf(NULL, 0, DAG, ad, host, service) + 1;
    char * dag = (char*)malloc(len);
    sprintf(dag, DAG, ad, host, service);
	return dag;
}

void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] -[l loops] [-s size] [-d delay]\n", name);
	printf("where:\n");
	printf(" -q       : quiet mode\n");
	printf(" -l loops : loop <loops> times and exit\n");
	printf(" -s size  : set packet size to <size>. if 0, uses random sizes\n");
	printf(" -d delay : delay for <delay> hundredths of a second between sends\n");
	printf("\n");
	exit(0);
}

void getConfig(int argc, char** argv)
{
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hqd:l:s:")) != -1) {
		switch (c) {
			case '?':
			case 'h':	// get help
				help(basename(argv[0]));
				break;
			case 'q':
				verbose = 0;
				break;
			case 'd':
				delay = atoi(optarg);
				if (delay < 0) delay = 0;
				break;
			case 'l':
				loops = atoi(optarg);
				if (loops < 0) loops = 0;
				break;
			case 's':
				pktSize = atoi(optarg);
				if (pktSize < 0) pktSize = 0;
				if (pktSize > 1024) pktSize = 1024;
				break;
			default:
				printf("hit default case\n");
				exit(0);
		}
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

void warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

}

void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "%s: exiting\n", TITLE);
	exit(ecode);
}

char *randomString(char *buf, int size)
{
	int i;
	static const char *filler = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static int initialized = 0;
	int samples = strlen(filler);

	if (!initialized) {
		srand(time(NULL));
		initialized = 1;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = 0;

	return buf;
}


int run(int sock)
{
	int size;
	int sent, received;
	char buf1[2048], buf2[2048];


	if (pktSize == 0)
		size = (rand() % 1023) + 1;
	else
		size = pktSize;
	randomString(buf1, size);

//	say("%d %s\n", strlen(buf), buf);
	if ((sent = Xsend(sock, buf1, size, 0)) < 0)
		die(-4, "Send error %d on socket %d\n", errno, sock);

	say("Sent %d bytes on socket %d\n", sent, sock);

	memset(buf2, sizeof(buf2), 0);
	if ((received = Xrecv(sock, buf2, sizeof(buf2), 0)) < 0)
		die(-5, "Receive error %d on socket %d\n", errno, sock);

	say("Received %d bytes on socket %d\n", received, sock);

	if (sent != received || strcmp(buf1, buf2) != 0)
		warn("received data did not equal sent data!\n");

	return 0;
}

void pausex()
{
	int t;
	if (delay == -1)
		return;
	else if (delay == 0)
		t = rand() % 1000000;
	else
		t = delay * 10000;
	usleep(t);
}

int main(int argc, char **argv)
{
	int ssock;
	char *sdag;

	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (!(sdag = createDAG(AD0, HID0, SID0))) {
		die(-1, "unable to create DAG\n");
	}
	say("Created DAG: \n%s\n", sdag);
	

	if ((ssock = Xsocket(XSOCK_STREAM)) < 0) {
		die(-2, "unable to create the server socket\n");
	}
	say("Created Xsocket\n");

	if (Xconnect(ssock, sdag) < 0) 
		die(-3, "unable to connect to the destination dag\n");
	
	say("Connected!\n");

	srand(time(NULL));

	if (loops == 0) {
		while(run(ssock) == 0) {
			pausex();
		}
	} else {
		for (int i = 0; i < loops; i++) {
			if (run(ssock) != 0)
				break;
			pausex();
		}
	}

	Xclose(ssock);
	free(sdag);
	return 0;
}
