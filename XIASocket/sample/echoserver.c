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
#include <errno.h>
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Echo Server"

#define DAG  "RE %s %s %s"
#define HID0 "HID:0000000000000000000000000000000000000000"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define SID0 "SID:0f00000000000000000000000000000000000777"

// global configuration options
int verbose = 1;

/*
** simple code to create a formatted DAG
**
** The dag should be free'd by the calling code when no longer needed
*/
char *createDAG(const char *ad, const char *host, const char *service)
{
	int len = snprintf(NULL, 0, DAG, ad, host, service) + 1;
    char * dag = (char*)malloc(len);
    sprintf(dag, DAG, ad, host, service);
	return dag;
}

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] \n", name);
	printf("where:\n");
	printf(" -q         : quiet mode\n");
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

	while ((c = getopt(argc, argv, "hq")) != -1) {
		switch (c) {
			case 'q':
				// turn off info messages
				verbose = 0;
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

int main(int argc, char *argv[])
{
    int acceptor, sock;
    char buf[2048];
	char *dag;
	int n;
    pid_t pid;

	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (!(dag = createDAG(AD0, HID0, SID0))) {
		die(-1, "unable to create DAG\n");
	}
	say("Created DAG: \n%s\n", dag);

    if ((acceptor = Xsocket(XSOCK_STREAM)) < 0) {
		die(-2, "unable to create the server socket\n");
	}

    if (Xbind(acceptor, dag) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

    while (1) {
    
		say("Xsock %4d waiting for a new connection.\n", acceptor);
    	if ((sock = Xaccept(acceptor)) < 0) {
			warn("Xsock %d accept failure! error = %d\n", acceptor, errno);
			continue;
		}

		say ("Xsock %4d new session\n", sock);
    	
    	pid = fork();
    
    	if (pid == 0) {  
    		
    		while (1) {
				memset(buf, 0, sizeof(buf));
			
				if ((n = Xrecv(sock, buf, sizeof(buf), 0)) < 0) {
					warn("Recv error on socket %d, closing connection\n", sock);
					break;
				}
			
				say("Xsock %4d received %d bytes\n", sock, n);

				if ((n = Xsend(sock, buf, n, 0)) < 0) {
					warn("send error on socket %d\n", sock);
					break;
				}

				say("Xsock %4d sent %d bytes\n", sock, n);
    		}
			printf("Xsock %4d closing\n", sock);
			Xclose(sock);
			return 0;
    	}
    }

	free(dag);
	Xclose(acceptor);
    return 0;
}

