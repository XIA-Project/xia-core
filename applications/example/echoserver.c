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
#ifdef __APPLE__
#include <libgen.h>
#endif
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#define VERSION "v1.0"
#define TITLE "XIA Echo Server"

#define MAX_XID_SIZE 100
#define STREAM_NAME "www_s.stream_echo.aaa.xia"
#define DGRAM_NAME "www_s.dgram_echo.aaa.xia"
#define SID_DGRAM   "SID:0f00000000000000000000000000000000008888"

// if no data is received from the client for this number of seconds, close the socket
#define WAIT_FOR_DATA	10

// global configuration options
int verbose = 1;
int stream = 1;
int dgram = 1;

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
				break;
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
	int n = 0;
	pid_t pid = getpid();

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

#ifdef USE_SELECT
	struct timeval tv;
	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;

	while (1) {
		memset(buf, 0, sizeof(buf));

		tv.tv_sec = WAIT_FOR_DATA;
		tv.tv_usec = 0;
		 if ((n = Xselect(sock + 1, &fds, NULL, NULL, &tv)) < 0) {
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
	}
#endif
	// if ((n = Xrecv(sock, buf, sizeof(buf), 0)) < 0) {
	// 	warn("Recv error on socket %d, closing connection\n", pid);
	// 	break;
	// } else if (n == 0) {
	// 	warn("%d client closed the connection\n", pid);
	// 	break;
	// }
	int count = 0;
	while ((count = Xrecv(sock, buf, sizeof(buf), 0)) != 0) {
		say("%5d received %d bytes\n", pid, count);
		n += count;
		int c = 0;
		if ((c = Xsend(sock, buf, count, 0)) < 0) {
			warn("%5d send error\n", pid);
			break;
		}

		say("%5d sent %d bytes\n", pid, c);
	}
	say("%5d received %d bytes in total\n", pid, n);
	say("%5d closing\n", pid);
	Xclose(sock);
}

static void reaper(int sig)
{
	if (sig == SIGCHLD) {
		while (waitpid(0, NULL, WNOHANG) > 0)
			;
	}
}

void echo_stream()
{
	char buf[XIA_MAX_DAG_STR_SIZE];
	int acceptor, sock;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	if (signal(SIGCHLD, reaper) == SIG_ERR) {
		die(-1, "unable to catch SIGCHLD");
	}

	say("Stream service started\n");

	if ((acceptor = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-2, "unable to create the stream socket\n");

	// Generate an SID to use
	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	// Get a list of interfaces and their corresponding addresses
	struct ifaddrs *ifaddr, *ifa;
	if(Xgetifaddrs(&ifaddr)) {
		die(-1, "unable to get interface addresses\n");
	}

	// See if we can find an address with rendezvous service
	std::string rvdagstr;
	for(ifa=ifaddr; ifa!=NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}
		if(ifa->ifa_flags & XIFA_RVDAG) {
			Graph rvdag((sockaddr_x *)ifa->ifa_addr);
			rvdagstr = rvdag.dag_string();
			break;
		}
	}

	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	if (rvdagstr.size() > XIA_XID_STR_SIZE) {
		hints.ai_flags |= XAI_DAGHOST;
		if(Xgetaddrinfo(rvdagstr.c_str(), sid_string, &hints, &ai)) {
			die(-1, "getaddrinfo with rvdag failed\n");
		}
	} else {
		if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0) {
			die(-1, "getaddrinfo failure!\n");
		}
	}

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	xia_ntop(AF_XIA, sa, buf, sizeof(buf));
	printf("\nStream DAG\n%s\n", buf);

	if (XregisterName(STREAM_NAME, sa) < 0 )
		die(-1, "error registering name: %s\n", STREAM_NAME);
	say("registered name: \n%s\n", STREAM_NAME);

	if (Xbind(acceptor, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

	Xlisten(acceptor, 5);

	while (1) {

		say("Xsock %4d waiting for a new connection.\n", acceptor);
		sockaddr_x sa;
		socklen_t sz = sizeof(sa);

		if ((sock = Xaccept(acceptor, (sockaddr*)&sa, &sz)) < 0) {
			warn("Xsock %d accept failure! error = %d\n", acceptor, errno);
			// FIXME: should we die here instead or try and recover?
			continue;
		}

		xia_ntop(AF_XIA, &sa, buf, sizeof(buf));
		say ("Xsock %4d new session\n", sock);
		say("peer:%s\n", buf);

		pid_t pid = Xfork();

		if (pid == -1) {
			die(-1, "FORK FAILED\n");

		} else if (pid == 0) {
			// close the parent's listening socket
			Xclose(acceptor);

			process(sock);
			exit(0);

		} else {
			// close the child's socket
			Xclose(sock);
		}
	}

	Xclose(acceptor);
}

void echo_dgram()
{
	int sock;
	char buf[XIA_MAXBUF];
	sockaddr_x cdag;
	socklen_t dlen;
	int n;

	say("Datagram service started\n");

	if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0)
		die(-2, "unable to create the datagram socket\n");

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_DGRAM, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	xia_ntop(AF_XIA, sa, buf, sizeof(buf));
	printf("\nDatagram DAG\n%s\n", buf);

	if (XregisterName(DGRAM_NAME, sa) < 0 )
		die(-1, "error registering name: %s\n", DGRAM_NAME);
	say("registered name: \n%s\n", DGRAM_NAME);

	if (Xbind(sock, (sockaddr *)sa, sizeof(sa)) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

	pid_t pid = 0;

	// only need to fork if doing stream echo at the same time
	if (stream == 1)
		pid = fork();

	if (pid == 0) {
		while (1) {
			say("Dgram Server waiting\n");

			dlen = sizeof(cdag);
			memset(buf, 0, sizeof(buf));
			if ((n = Xrecvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&cdag, &dlen)) < 0) {
				warn("Recv error on socket %d, closing connection\n", pid);
				break;
			}

			say("dgram received %d bytes\n", n);

			if ((n = Xsendto(sock, buf, n, 0, (struct sockaddr *)&cdag, dlen)) < 0) {
				warn("%5d send error\n", pid);
				break;
			}

			say("dgram sent %d bytes\n", n);
		}

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
