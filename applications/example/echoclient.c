/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

// FIXME: clean up the duplicate loop code
// TODO: add option for a static buffer instead of creating random data for perf testing
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#ifdef __APPLE__
#include <libgen.h>
#endif
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Echo Client"

#define STREAM_NAME "www_s.stream_echo.aaa.xia"

#define MAX_BUF_SIZE 62000


// FIXME: clean up globals and move into a structure or similar
// global configuration options
int verbose = 1;	// display all messages
int delay = -1;		// don't delay between loops
int loops = 1;		// only do 1 pass
int pktSize = 512;	// default pkt size
int reconnect = 0;	// don't reconnect between loops
int threads = 1;	// just a single thread
int terminate = 0;  // sighandler sets to 1 if we should quit

struct addrinfo *ai;
sockaddr_x *sa;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] -[l loops] [-s size] [-d delay] [-r recon] [-t threads]\n", name);
	printf("where:\n");
	printf(" -q : quiet mode\n");
	printf(" -l loops : loop <loops> times and exit\n");
	printf(" -s size : set packet size to <size>. if 0, uses random sizes\n");
	printf(" -d delay : delay for <delay> hundredths of a second between sends\n");
	printf(" -r recon : reconnect to the echo server every recon sends\n");
	printf(" -t threads : start up the specified # of threads\n");
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hqd:l:s:r:t:")) != -1) {
		switch (c) {
			case '?':
			case 'h':
				// Help Me!
				help(basename(argv[0]));
				break;
			case 'q':
				// turn off info messages
				verbose = 0;
				break;
			case 'd':
				// pause for <delay> hundredths of a second between operations
				// if 0, pause for a random period between 0 and 1 second
				delay = atoi(optarg) * 10000; // convert to hundredths
				if (delay < 0) delay = 0;
				break;
			case 'l':
				// loop <loops> times and exit
				// if 0, loop forever
				loops = atoi(optarg);
				if (loops < 0) loops = 0;
				break;
			case 's':
				// send pacets of size <size> maximum is 1024
				// if 0, send random sized packets
				pktSize = atoi(optarg);
				if (pktSize < 0) pktSize = 0;
				 if (pktSize > MAX_BUF_SIZE) pktSize = MAX_BUF_SIZE;
				break;
			case 'r':
				// close and reopen the connection every <reconnect> operations
				// if 0, don't bother to do it
				reconnect = atoi(optarg);
				if (reconnect < 0) reconnect = 0;
				break;
			case 't':
				// start up <threads> threads each using the other settings
				// for it's configuration
				threads = atoi(optarg);
				if (threads < 1) threads = 1;
				if (threads > 100) threads = 100;
				break;
			default:
				help(basename(argv[0]));
				break;
		}
	}
}

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

}

/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}

/*
** create a semi-random alphanumeric string of the specified size
*/
char *randomString(char *buf, int size)
{
	int i;
	static const char *filler = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static int refresh = 1;
	int samples = strlen(filler);

	if (!(--refresh)) {
		// refresh rand every now and then so it doesn't degenerate too much
		// use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = 0;

	return buf;
}

/*
** do one send/receive operation on the specified socket
*/
int process(int sock)
{
	int size;
	int sent = 0;
	int received = 0;
	char buf1[MAX_BUF_SIZE + 1], buf2[MAX_BUF_SIZE + 1];

	if (pktSize == 0)
		size = (rand() % MAX_BUF_SIZE) + 1;
	else
		size = pktSize;
	randomString(buf1, size);

	int count = size;

	sent = Xsend(sock, buf1, count, 0);
	if (sent < 0)
	{
		die(-4, "Send error %d on socket %d\n", errno, sock);
	}

	say("Xsock %4d sent %d of %d bytes\n", sock, sent, size);

	int rc;
	struct pollfd pfds[2];
	pfds[0].fd = sock;
	pfds[0].events = POLLIN;
	if ((rc = Xpoll(pfds, 1, 50000)) <= 0) {
		die(-5, "Poll returned %d\n", rc);
	}

	memset(buf2, 0, sizeof(buf2));
	count = 0;
	while (sent != received && (count = Xrecv(sock, &buf2[received], sizeof(buf2) - received, 0)) > 0) {
		say("%5d received %d bytes\n", sock, count);
		received += count;
		buf2[received] = 0;
	}

	say("Xsock %4d received %d bytes in total\n", sock, received);

	if (sent != received || strcmp(buf1, buf2) != 0)
		warn("Xsock %4d received data different from sent data! (bytes sent/recv'd: %d/%d)\n",
				sock, sent, received);

	return 0;
}

/*
** do a short pause between operations
*/
void pausex()
{
	int t;
	if (delay == -1)
		// default - don't pause at all
		return;
	else if (delay == 0)
		// pause for some random period less than a second
		t = rand() % 1000000;
	else
		// pause for the specfied number of hundredths of a second
		t = delay;
	usleep(t);
}

/*
** create a socket and connect to the remote server
*/
int connectToServer()
{
	int ssock;
	if ((ssock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		die(-2, "unable to create the server socket\n");
	}
	say("Xsock %4d created\n", ssock);

	if (Xconnect(ssock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0)
		die(-3, "unable to connect to the destination dag\n");

	say("Xsock %4d connected\n", ssock);

	return ssock;
}

/*
** the main loop for talking to the remote server
** when threading is enabled, one of these will run in each thread
**
** The parameter and return code are there to satisify the thread library
*/
void *mainLoop(void * /* dummy */)
{
	int ssock;
	int count = 0;
	int printcount = 1;

	if (loops == 1)
		printcount = 0;

	ssock = connectToServer();

	while (!terminate) {

		if (printcount)
			say("Xsock %4d loop #%d\n", ssock, count);

		if (process(ssock) != 0)
			break;

		pausex();

		count++;
		if (reconnect != 0) {
			if (count % reconnect == 0) {
				// time to close and reopen the socket
				Xclose(ssock);
				say("Xsock %4d closed\n", ssock);
				ssock = connectToServer();
			}
		}
		if (loops > 0 && count == loops)
				break;
	}
	Xclose(ssock);
	say("Xsock %4d closed\n", ssock);

	return NULL;
}

void quithandler(int)
{
	terminate = 1;
}

/*
** where it all happens
*/
int main(int argc, char **argv)
{
	char buf[XIA_MAX_DAG_STR_SIZE];

	srand(time(NULL));
	getConfig(argc, argv);

//	signal(SIGINT, quithandler);


	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (Xgetaddrinfo(STREAM_NAME, NULL, NULL, &ai) != 0)
		die(-1, "unable to lookup name %s\n", STREAM_NAME);
	sa = (sockaddr_x*)ai->ai_addr;

	xia_ntop(AF_XIA, sa, buf, sizeof(buf));
	printf("\n%s\n", buf);

	if (threads == 1)
		// just do it
		mainLoop(NULL);
	else {
		pthread_t *clients = (pthread_t*)malloc(threads * sizeof(pthread_t));

		if (!clients)
			die(-5, "Unable to allocate threads\n");

		for (int i = 0; i < threads; i++) {
			pthread_create(&clients[i], NULL, mainLoop, NULL);
		}
		for (int i = 0; i < threads; i++) {
			pthread_join(clients[i], NULL);
		}

		free(clients);
	}

	return 0;
}
