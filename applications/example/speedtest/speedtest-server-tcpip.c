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
#include <sys/socket.h> // listen()
#include <unistd.h> // fork()
#include <string.h> // strerror(), memset()
#include <arpa/inet.h> // htonl()


#define VERSION "v1.0"
#define TITLE "IP Speed Test Server"


// if no data is received from the client for this number of seconds, close the
// socket
#define WAIT_FOR_DATA	10
#define MAXBUFLEN 15600

// global configuration options
int verbose = 0;
uint16_t tcpPort = 8888;


/*
** display cmd line options and exit
*/
void help(const char *name){

	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] \n", name);
	printf("where:\n");
	printf(" -v : verbose\n");
	printf(" -p : TCP port to listen on, default %u\n", tcpPort);
	printf("\n");
	exit(0);
}


/*
** configure the app
*/
void getConfig(int argc, char** argv){
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hvp:")) != -1) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 'p':
				tcpPort = atoi(optarg);
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
void processClient(int sockfd){

	char buf[MAXBUFLEN + 1];
	int nrcvdBytes;
	pid_t pid = getpid();

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
/*
#ifdef USE_SELECT
	struct timeval tv;
	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;
#endif
*/
    memset(buf, 0, sizeof(buf));
    unsigned long long ntotalBytes = 0;
    time_t startTime = time(NULL);

   	while (1) {

/*
#ifdef USE_SELECT
		tv.tv_sec = WAIT_FOR_DATA;
		tv.tv_usec = 0;
		 if ((n = Xselect(sockfd + 1, &fds, NULL, NULL, &tv)) < 0) {
			 printf("%5d Select failed, closing...\n", pid);
			 break;

		 } else if (n == 0) {
			 // we timed out, close the socket
			 printf("%5d timed out on recv\n", pid);
			 break;
		 } else if (!FD_ISSET(sockfd, &fds)) {
			 // this shouldn't happen!
			 die(-4, "something is really wrong, exiting\n");
		 }
#endif
*/
		if ((nrcvdBytes = recv(sockfd, buf, sizeof(buf), 0)) < 0) {
			printf("Recv error on socket %d, closing connection\n", pid);
			break;
		} else if (nrcvdBytes == 0) {
			printf("%d client closed the connection\n", pid);
			break;
		}

        if (verbose){
            printf("sock %4d received %d bytes\n", sockfd, nrcvdBytes);
        }
        
        ntotalBytes += nrcvdBytes;
   	}
    
    time_t endTime = time(NULL);
    time_t deltat = endTime-startTime;
    double throughput = ((double)ntotalBytes)/(deltat)/1e6;
    printf("Test complete: %us @ %.2f MB/s\n", (unsigned int)deltat, \
            throughput);

	close(sockfd);
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

	if (signal(SIGCHLD, reaper) == SIG_ERR){
		die(-1, "unable to catch SIGCHLD");
	}

    if (verbose){
        printf("Stream service started\n");
    }

	if ((acceptorfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		die(-2, "unable to create the stream socket\n");
    }

    struct sockaddr_in servAddr; 
    memset(&servAddr, '0', sizeof(servAddr));

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(tcpPort);

	if (bind(acceptorfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
		die(-3, "unable to bind to the dag\n");
	}

	listen(acceptorfd, 5);

	while (1) {

        if (verbose){
            printf("sock %4d waiting for a new connection.\n", acceptorfd);
        }
        
		if ((sockfd = accept(acceptorfd, (struct sockaddr*)NULL, NULL)) < 0){
			printf("sock %d accept failure! error: %s\n", acceptorfd, \
                strerror(errno));
			// FIXME: should we die here instead or try and recover?
			continue;
		}

        printf("Sock %4d new session\npeer\n", sockfd);

		pid_t pid = fork();

		if (pid == -1) {
			die(-1, "FORK FAILED\n");

		} else if (pid == 0) {
			// close the parent's listening socket
			close(acceptorfd);

			processClient(sockfd);
			exit(0);

		} else {
			// close the child's socket
			close(sockfd);
		}
	}

	close(acceptorfd);

	return 0;
}
