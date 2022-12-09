/* ts=4 */
/*
** Copyright 2015 Carnegie Mellon University
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

#include <stdint.h> // uint*_t
#include <stdio.h> // printf()
#include <unistd.h> // getopt()
#include <stdarg.h> // va_*()
#include <errno.h> // errno
#include <pthread.h> // pthread_*()
#include <signal.h> // signal()
#include <string.h> // strerror()

#include "Xsocket.h"
#include "dagaddr.hpp"

#define VERSION "v1.0"
#define TITLE "XIA Speed Test Client"

#define STREAM_NAME "www_s.stream_echo.aaa.xia"

// global configuration options
uint8_t verbose = 0;	// display all messages
unsigned int pktSize = 1024;	// default pkt size (bytes)
uint8_t terminate = 0;  // set to 1 when it's time to quit
useconds_t sleepTime = 0;


struct addrinfo *ai;
sockaddr_x *sa;

/**
 * display cmd line options and exit
 */
void help(const char *name){

	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-v] [-s size]\n", name);
	printf("where:\n");
	printf(" -v : verbose mode\n");
	printf(" -s size : set packet size to <size>, default %u bytes\n", pktSize);
	printf(" -i interval : between sends, default %u microsecs \n", sleepTime);
	printf("\n");
	exit(0);
}


/**
 * configure the app
 */
void getConfig(int argc, char** argv){

	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hvs:i:")) != -1) {
		switch (c) {
			case '?':
			case 'h':
				// Help Me!
				help(basename(argv[0]));
				break;
			case 'v':
				// turn on info messages
				verbose = 1;
				break;
			case 's':
				// send packets of size <size> maximum is 1024
				// if 0, send random sized packets
				pktSize = atoi(optarg);
				if (pktSize < 1) pktSize = 1;
				if (pktSize > XIA_MAXBUF) pktSize = XIA_MAXBUF;
				break;
			case 'i':
				// interval sleep time
				// if 0, send random sized packets
				sleepTime = atoi(optarg);
				break;
			default:
				help(basename(argv[0]));
				break;
		}
	}
}


/**
 * write the message to stdout, and exit the app
 */
void die(int ecode, const char *fmt, ...){

	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}


/**
 * create a semi-random alphanumeric string of the specified size
 */
char *randomString(char *buf, int size){

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


/**
 * the main loop thread
 * The parameter and return code are there to satisify the thread library
 */
void *mainLoopThread(void * /*arg*/){

	int ssock;
    unsigned long nmsgs = 0, totalNbytes=0;

    if ((ssock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0){
		die(-2, "unable to create the server socket\n");
	}
    
    if (verbose){
        printf("Xsock %4d created\n", ssock);
    }

	if (Xconnect(ssock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0){
		die(-3, "unable to connect to the destination dag\n");
    }

    if (verbose){
        printf("Xsock %4d connected\n", ssock);
    }

	char sndBuf[XIA_MAXBUF + 1];
	int nsntBytes=0;

	randomString(sndBuf, pktSize); // set the snd buffer to a random string

	while (!terminate) { // the main loop proper

        if ((nsntBytes = Xsend(ssock, sndBuf, pktSize, 0)) < 0){
            die(-4, "Send error %s on socket %d\n", strerror(errno), ssock);
        }
        nmsgs += 1;
        totalNbytes += nsntBytes;

        if (verbose){
            printf("Xsock %4d sent %d of %db (total %lu msgs %lub)\n", \
                ssock, nsntBytes, pktSize, nmsgs, totalNbytes);
        }
        usleep(sleepTime);
	}
    
	Xclose(ssock);
    
    if (verbose){
        printf("Xsock %4d closed\n", ssock);
    }

	return NULL;
}


static void reaper(int /*sig*/){

    terminate = 1;
}


/**
 * where it all happens
 */
int main(int argc, char **argv){

	srand(time(NULL));
	getConfig(argc, argv);

    if (verbose){
        printf("\n%s (%s): started\n", TITLE, VERSION);
    }

	if (signal(SIGINT, reaper) == SIG_ERR){
		die(-1, "unable to install signal handler");
	}


	if (Xgetaddrinfo(STREAM_NAME, NULL, NULL, &ai)){
		die(-1, "unable to lookup name %s\n", STREAM_NAME);
    }
    
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);

    pthread_t mainThreadId;

    if (pthread_create(&mainThreadId, NULL, mainLoopThread, NULL)){
        die(-1, "unable to create thread: %s\n", strerror(errno));
    }


    if(pthread_join(mainThreadId, NULL)){
            die(-1, "unable to join thread: %s\n", strerror(errno));
    }

	return 0;
}
