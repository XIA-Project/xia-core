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
#define STREAM_NAME "www_s.stream_echo.aaa.xia"

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


/*
** get data from the client and return it
** run in a child process that was forked off from the main process.
*/
void processClient(int sock){

	char buf[XIA_MAXBUF + 1];
	int nrcvdBytes;
	pid_t pid = getpid();

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

#ifdef USE_SELECT
	struct timeval tv;
	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;
#endif

    memset(buf, 0, sizeof(buf));
    unsigned long long ntotalBytes = 0;
    time_t startTime = time(NULL);
    
    unsigned long nmsgs = 0, totalNbytes = 0;

   	while (1) {


#ifdef USE_SELECT
		tv.tv_sec = WAIT_FOR_DATA;
		tv.tv_usec = 0;
		 if ((n = Xselect(sock + 1, &fds, NULL, NULL, &tv)) < 0) {
			 printf("%5d Select failed, closing...\n", pid);
			 break;

		 } else if (n == 0) {
			 // we timed out, close the socket
			 printf("%5d timed out on recv\n", pid);
			 break;
		 } else if (!FD_ISSET(sock, &fds)) {
			 // this shouldn't happen!
			 die(-4, "something is really wrong, exiting\n");
		 }
#endif

		if ((nrcvdBytes = Xrecv(sock, buf, sizeof(buf), 0)) < 0) {
			printf("Recv error on socket %d, closing connection\n", pid);
			break;
		} else if (nrcvdBytes == 0) {
			printf("%d client closed the connection\n", pid);
			break;
		}
        nmsgs += 1;
        totalNbytes += nrcvdBytes;
        if (verbose){
            printf("Xsock %4d received %db (total %lu msgs, %lub)\n", sock, \
                nrcvdBytes, nmsgs, totalNbytes);
        }
        
        ntotalBytes += nrcvdBytes;
   	}

    const time_t endTime = time(NULL);
    const time_t deltat = endTime-startTime;
    const double throughput = ((double)ntotalBytes*8)/(deltat)/1e6;
    printf("Test complete: %us @ %.3f Mbps\n", (unsigned int)deltat, \
        throughput);

	Xclose(sock);
}


static void reaper(int sig){

	if (sig == SIGCHLD) {
		while (waitpid(0, NULL, WNOHANG) > 0)
			;
	}
}


int main(int argc, char *argv[]){

	getConfig(argc, argv);

    if (verbose){
        printf("\n%s (%s): started\n", TITLE, VERSION);
    }

    // wait for them clients
	int acceptorfd, sockfd;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	if (signal(SIGCHLD, reaper) == SIG_ERR){
		die(-1, "unable to catch SIGCHLD");
	}

    if (verbose){
        printf("Stream service started\n");
    }

	if ((acceptorfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		die(-2, "unable to create the stream socket\n");
    }

	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;

	if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0){
		die(-1, "getaddrinfo failure!\n");
    }

	Graph g((sockaddr_x*)ai->ai_addr);

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	printf("\nStream DAG\n%s\n", g.dag_string().c_str());

    if (XregisterName(STREAM_NAME, sa) < 0 ){
    	die(-1, "error registering name: %s\n", STREAM_NAME);
    }
    
    if (verbose){
        printf("registered name: \n%s\n", STREAM_NAME);
    }

	if (Xbind(acceptorfd, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0){
		die(-3, "unable to bind to the dag\n");
	}

	Xlisten(acceptorfd, 5);

	while (1) {

        if (verbose){
            printf("Xsock %4d waiting for a new connection.\n", acceptorfd);
        }
        
		sockaddr_x sa;
		socklen_t sz = sizeof(sa);

		if ((sockfd = Xaccept(acceptorfd, (sockaddr*)&sa, &sz)) < 0){
			printf("Xsock %d accept failure! error = %d\n", acceptorfd, errno);
			// FIXME: should we die here instead or try and recover?
			continue;
		}

		Graph g(&sa);
        printf("Xsock %4d new session\npeer:%s\n", sockfd, \
            g.dag_string().c_str());
        

		pid_t pid = Xfork();

		if (pid == -1) {
			die(-1, "FORK FAILED\n");

		} else if (pid == 0) {
			// close the parent's listening socket
			Xclose(acceptorfd);

			processClient(sockfd);
			exit(0);

		} else {
			// close the child's socket
			Xclose(sockfd);
		}
	}

	Xclose(acceptorfd);

	return 0;
}
