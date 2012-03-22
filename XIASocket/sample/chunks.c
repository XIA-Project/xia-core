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
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Chunk Server"

#define DAG  "RE %s %s CID:1234%036d"
#define HID0 "HID:0000000000000000000000000000000000000000"
#define AD0   "AD:1000000000000000000000000000000000000000"

// global configuration options
int verbose = 1;
int numCids = 5;

/*
** simple code to create a formatted DAG
**
** The dag should be free'd by the calling code when no longer needed
*/
char *createDAG(const char *ad, const char *host, unsigned id)
{
	int len = snprintf(NULL, 0, DAG, ad, host, id) + 1;

	char * dag = (char*)malloc(len);
	sprintf(dag, DAG, ad, host, id);
	return dag;
}

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] [-c <count>] \n", name);
	printf("where:\n");
	printf(" -c <count>: create <count> cids (default = 5)\n");
	printf(" -q : quiet mode\n");
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

	while ((c = getopt(argc, argv, "hqc:")) != -1) {
		switch (c) {
			case 'q':
				// turn off info messages
				verbose = 0;
				break;
			case 'c':
				numCids = atoi(optarg);
				if (numCids <= 0) numCids = 1;
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help(basename(argv[0]));
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
		//  use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = 0;

	return buf;
}

int main(int argc, char **argv)
{
	int sock;
  	char data[1024];
	
	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if ((sock = Xsocket(XSOCK_CHUNK)) < 0)
		die(-1, "Unable to create a socket\n");

	say("Creating %d CIDs\n", numCids);
	for (int i = 0;  i < numCids; i++) {
		char *cdag =  createDAG(AD0, HID0, i);
		if (!cdag)
			die(-2, "Unable to allocate dag for cid %d\n", i);

		say ("%s\n", cdag);

		randomString(data, sizeof(data));

    	if (XputCID(sock, data, strlen(data), 0, cdag, strlen(cdag)) < 0)
			die(-3, "failed putting the CID into the cache\n");
		
		free(cdag);
	}

	Xclose(sock);

    return 0;
}

