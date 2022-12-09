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
#include <sys/wait.h>
#include <time.h> // time()

#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#define VERSION "v1.0"
#define TITLE "XIA Speed Test Server"

#define MAX_XID_SIZE 100
#define DGRAM_NAME "www_s.dgram_echo.aaa.xia"
#define SID_DGRAM   "SID:0f00000000000000000000000000000000008888"

// if no data is received from the client for this number of seconds, close the
// socket
#define WAIT_FOR_DATA	10

// global configuration options
int verbose = 0;


/*
** display cmd line options and exit
*/
void help(const char *name){

	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] \n", name);
	printf("where:\n");
	printf(" -v : verbose\n");
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv){
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hv")) != -1) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}
}


/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...){

	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}


int main(int argc, char *argv[]){

	getConfig(argc, argv);

    if (verbose){
        printf("\n%s (%s): started\n", TITLE, VERSION);
    }

    // wait for them clients
	int sock;
	char buf[XIA_MAXBUF];
	sockaddr_x cdag;
	socklen_t dlen;
	int n;
    
    pid_t pid = getpid();


	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		die(-2, "unable to create the datagram socket\n");

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_DGRAM, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	Graph g((sockaddr_x*)ai->ai_addr);
	printf("\nDatagram DAG\n%s\n", g.dag_string().c_str());

    if (XregisterName(DGRAM_NAME, sa) < 0 ){
    	die(-1, "error registering name: %s\n", DGRAM_NAME);
    }

    if (verbose){
        printf("registered name: \n%s\n", DGRAM_NAME);
    }

	if (Xbind(sock, (sockaddr *)sa, sizeof(sa)) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

    time_t lastTime = time(NULL);
    unsigned long long nrcvdBytesTotal=0, nrcvdPacketsTotal=0, nrcvdBytesSec=0;

	while (1){

        dlen = sizeof(cdag);
        memset(buf, 0, sizeof(buf));

        if ((n = Xrecvfrom(sock, buf, sizeof(buf), 0, \
            (struct sockaddr *)&cdag, &dlen)) < 0) {
            printf("Recv error on socket %d, closing connection\n", pid);
            break;
        }

        nrcvdBytesTotal += n;
        nrcvdBytesSec += n;
        nrcvdPacketsTotal++;

        if (verbose){
            printf("dgram received %db (total %llub, %llu packets)\n", n, nrcvdBytesTotal, nrcvdPacketsTotal);
        }

        const time_t newTime = time(NULL);

        if (newTime != lastTime){

            const time_t deltat = newTime-lastTime;
            const double throughput = ((double)nrcvdBytesSec*8)/(deltat)/1e6;
            printf("%us @ %.4f Mbps, total packets=%llu (%llub)\n", \
                (unsigned int)deltat, throughput, nrcvdPacketsTotal, \
                nrcvdBytesTotal);

            lastTime = newTime;
            nrcvdBytesSec = 0;
        }
	}

    Xclose(sock);

	return 0;
}
