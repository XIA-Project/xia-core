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
#define TITLE "XIA Chunk Client"

#define DAG  "RE ( %s %s ) CID:1234%036d"
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
	printf(" -c <count>: retrieve up to <count> cids (default = 5)\n");
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

const char *statusString(int status)
{
	switch(status) {
		case WAITING_FOR_CHUNK:
			return "Waiting";
		case READY_TO_READ:
			return "Ready";
	}
	return "Request Failed";
}

void doCIDStatus(int sock, struct cDAGvec *dagv)
{
   int status;

   say("getting CID list status\n");
    if (XgetCIDListStatus(sock, dagv, numCids) < 0)
		die(-5, "getCIDListStatus failed\n");

	say("CID list status: ");
	for (int i = 0; i < numCids; i++) {
		if (i != numCids - 1)
			say("%d:%s, ", i, statusString(dagv[i].status));
		else
			say("%d:%s", i, statusString(dagv[i].status));
	}
	say("\n");
    
   say("getting individual CID status\n");
	for (int i = 0; i < numCids; i++) {
    	if ((status = XgetCIDStatus(sock, dagv[i].cDAG, dagv[i].dlen)) < 0)
			die(-6, "XgetCIDStatus failed\n");

    	say("CID%d status= %s\n", i, statusString(status));
	}
}

int main(int argc, char **argv)
{
    int sock;
	int n;
	char data[2048];

    struct cDAGvec *dagv;
	

	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	say("creating list of %d CIDs\n", numCids);
	dagv = (struct cDAGvec *)calloc(numCids, sizeof(struct cDAGvec));
	if (!dagv)
		die(-1, "Unable to allocate dags\n");

	for (int i = 0; i < numCids; i++) {
		char *dag = createDAG(AD0, HID0, i);
		if (!dag)
			die(-2, "Unable to allocate dag %d\n", i);

		dagv[i].cDAG = dag;
		dagv[i].dlen = strlen(dag);
	}
    
	say("creating socket\n");
	if ((sock = Xsocket(XSOCK_CHUNK)) < 0)
		die(-3, "Unable to allocate socket\n");

     
	say("requesting list of CIDs\n");
    if (XgetCIDList(sock, dagv, numCids - 1) < 0)
		die(-4, "Unable to get CID list\n");

	say("requesting single CID\n");
    if( XgetCID(sock, dagv[numCids - 1].cDAG, dagv[numCids - 1].dlen) < 0)
		die(-4, "Unable to get CID\n");

	doCIDStatus(sock, dagv);
    
	say("\nGetting the CIDs\n");
	for (int i = 0; i < numCids; i++) {
	 	if ((n = XreadCID(sock, data, sizeof(data), 0, dagv[i].cDAG, dagv[i].dlen)) < 0) {
			warn("XReadCID failed\n");
			continue;
		}
		say("CID%d contains %d bytes\n", i, n);
	}
    
	doCIDStatus(sock, dagv);

	for (int i = 0; i < numCids; i++)
		free(dagv[i].cDAG);
	free(dagv);

    Xclose(sock);
    return 0;
}

