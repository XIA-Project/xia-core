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

#define MAX_XID_SIZE 100
#define DAG  "RE %s %s %s"
#define STREAM_NAME "www_s.stream_echo.aaa.xia"
#define DGRAM_NAME "www_s.dgram_echo.aaa.xia"
#define SID_STREAM  "SID:0f00000000000000000000000000000000000888"
#define SID_DGRAM   "SID:0f00000000000000000000000000000000008888"

// if no data is received from the client for this number of seconds, close the socket
#define WAIT_FOR_DATA	20

// global configuration options
int verbose = 1;
int stream = 1;
int dgram = 1;

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
	printf(" -q : quiet mode\n");
	printf(" -s : stream echo\n");
	printf(" -d : datagram echo\n");
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;
	int s = 0;
	int d = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "hqsd")) != -1) {
		switch (c) {
			case 'q':
				// turn off info messages
				verbose = 0;
				break;
			case 's':
				s = 1;
				break;
			case 'd':
				d = 1;
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help(basename(argv[0]));
		}
	}

	if (s || d) {
		stream = s;
		dgram = d;
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
** get data from the client and return it
** run in a child process that was forked off from the main process.
*/
void process(int sock)
{
	char buf[XIA_MAXBUF + 1];
	int n;
	pid_t pid = getpid();

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	struct timeval tv;
	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;

   	while (1) {
		memset(buf, 0, sizeof(buf));

	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;
		 if ((n = select(sock + 1, &fds, NULL, NULL, &tv)) < 0) {
			 warn("%5d Select failed, closing...\n", pid);
			 break;
		 
		 } else if (n == 0) {
			 // we timed out, close the socket
			 say("%5d timed out on recv\n", pid);
			 break;
		 } else if (!FD_ISSET(sock, &fds)) {
			 // this shouldn't happen!
			 die(-4, "something is really wrong, exiting\n");
		 }
	
		if ((n = Xrecv(sock, buf, sizeof(buf), 0)) < 0) {
			warn("Recv error on socket %d, closing connection\n", pid);
			break;
		}
			
		say("%5d received %d bytes\n", pid, n);

		if ((n = Xsend(sock, buf, n, 0)) < 0) {
			warn("%5d send error\n", pid);
			break;
		}

		say("%5d sent %d bytes\n", pid, n);
   	}
	say("%5d closing\n", pid);
	Xclose(sock);
}

void echo_stream()
{
	char myAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE];
	int acceptor, sock;
	char *dag;

	say("Stream service started\n");

	if ((acceptor = Xsocket(XSOCK_STREAM)) < 0)
		die(-2, "unable to create the stream socket\n");

    // read the localhost AD and HID
    if ( XreadLocalHostAddr(acceptor, myAD, sizeof(myAD), myHID, sizeof(myHID)) < 0 )
    	die(-1, "Reading localhost address\n");

	if (!(dag = createDAG(myAD, myHID, SID_STREAM)))
		die(-1, "unable to create DAG: %s\n", dag);
	say("Created DAG: \n%s\n", dag);

    if (XregisterName(STREAM_NAME, dag) < 0 )
    	die(-1, "error registering name: %s\n", STREAM_NAME);
	say("registered name: \n%s\n", STREAM_NAME);

	if (Xbind(acceptor, dag) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

	while (1) {

		say("Xsock %4d waiting for a new connection.\n", acceptor);
		if ((sock = Xaccept(acceptor)) < 0) {
			warn("Xsock %d accept failure! error = %d\n", acceptor, errno);
			// FIXME: should we die here instead or try and recover?
			continue;
		}

		say ("Xsock %4d new session\n", sock);

		pid_t pid = fork();

		if (pid == 0) {  
			process(sock);
			exit(0);

		} else {
			// FIXME: we need to close the socket in the main process or the file
			// descriptor limit will be hit. But if Xclose is called, the connection
			// is torn down in click as well keeping the child process from using it.
			// for now use a regular close to shut it here without affecting the child.
			close(sock);
		}
	}

	free(dag);
	Xclose(acceptor);
}

void echo_dgram()
{
	int sock;
	char *dag;
	char buf[XIA_MAXBUF];
	char cdag[1024]; // client's dag
	char myAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE];
	size_t dlen;
	int n;

	say("Datagram service started\n");

	if ((sock = Xsocket(XSOCK_DGRAM)) < 0)
		die(-2, "unable to create the datagram socket\n");

    // read the localhost AD and HID
    if ( XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID)) < 0 )
    	die(-1, "Reading localhost address\n");

	if (!(dag = createDAG(myAD, myHID, SID_DGRAM)))
		die(-1, "unable to create DAG: %s\n", dag);
	say("Created DAG: \n%s\n", dag);

    if (XregisterName(DGRAM_NAME, dag) < 0 )
    	die(-1, "error registering name: %s\n", DGRAM_NAME);
	say("registered name: \n%s\n", DGRAM_NAME);

	if (Xbind(sock, dag) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

	pid_t pid = fork();
	if (pid == 0) {
		while (1) {
			say("Dgram Server waiting\n");

			dlen = sizeof(cdag);
			memset(buf, 0, sizeof(buf));
			if ((n = Xrecvfrom(sock, buf, sizeof(buf), 0, cdag, &dlen)) < 0) {
				warn("Recv error on socket %d, closing connection\n", pid);
				break;
			}
			
			say("dgram received %d bytes\n", n);

			if ((n = Xsendto(sock, buf, n, 0, cdag, dlen)) < 0) {
				warn("%5d send error\n", pid);
				break;
			}

			say("dgram sent %d bytes\n", n);
		}

		free(dag);
		Xclose(sock);
	}
}

int main(int argc, char *argv[])
{
	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	// if configured, fork off a process to handle datagram connections
	if (dgram)
		echo_dgram();

	// if configured, fork off processes as needed to handle stream connections
	if (stream)
		echo_stream();

	return 0;
}
