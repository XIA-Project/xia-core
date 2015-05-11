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
#include "dagaddr.hpp"
#include "xssl.h"

#define VERSION "v1.0"
#define TITLE "XIA Echo Server"

#define MAX_XID_SIZE 100
#define STREAM_NAME "www_s.stream_echo.aaa.xia"
#define DGRAM_NAME "www_s.dgram_echo.aaa.xia"

// if no data is received from the client for this number of seconds, close the socket
#define WAIT_FOR_DATA	10

// global configuration options
int verbose = 1;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] \n", name);
	printf("where:\n");
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

	while ((c = getopt(argc, argv, "hqsd")) != -1) {
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
** get data from the client and return it
** run in a child process that was forked off from the main process.
*/
void process(int sock, XSSL_CTX *ctx)
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

	/* set up XSSL connection */
	XSSL *xssl = XSSL_new(ctx);
	if (xssl == NULL) {
		die(-6, "Error creating new XSSL object\n");
	}
	if (XSSL_set_fd(xssl, sock) != 1) {
		die(-7, "Error setting XSSL sockfd\n");
	}
	if (XSSL_accept(xssl) != 1) {
		die(-8, "Error accepting XSSL connection\n");
	}

   	while (1) {
		memset(buf, 0, sizeof(buf));

	tv.tv_sec = WAIT_FOR_DATA;
	tv.tv_usec = 0;
		 if ((n = Xselect(xssl->sockfd + 1, &fds, NULL, NULL, &tv)) < 0) {
			 warn("%5d Select failed, closing...\n", pid);
			 break;

		 } else if (n == 0) {
			 // we timed out, close the socket
			 say("%5d timed out on recv\n", pid);
			 break;
		 } else if (!FD_ISSET(xssl->sockfd, &fds)) {
			 // this shouldn't happen!
			 die(-4, "something is really wrong, exiting\n");
		 }

		if ((n = XSSL_read(xssl, buf, sizeof(buf))) < 0) {
			warn("Recv error on socket %d, closing connection\n", pid);
			break;
		}

		say("%5d received %d bytes\n", pid, n);

		if ((n = XSSL_write(xssl, buf, n)) < 0) {
			warn("%5d send error\n", pid);
			break;
		}

		say("%5d sent %d bytes\n", pid, n);
   	}
	say("%5d closing\n", pid);
	XSSL_shutdown(xssl);
	Xclose(sock);
	XSSL_free(xssl);
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
	int acceptor, sock;

	if (signal(SIGCHLD, reaper) == SIG_ERR) {
		die(-1, "unable to catch SIGCHLD");
	}
	say("Stream service started\n");

	/* Prepare XSSL context and get SID (based on RSA key) */
	XSSL_CTX *ctx = XSSL_CTX_new();
	if (ctx == NULL) {
		die(-5, "Unable to create new XSSL_CTX\n");
	}
	char *SID = SID_from_keypair(ctx->keypair);

	/* Prepare listen socket, binding to generated SID */
	if ((acceptor = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-2, "unable to create the stream socket\n");

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	Graph g((sockaddr_x*)ai->ai_addr);

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	printf("\nStream DAG\n%s\n", g.dag_string().c_str());

    if (XregisterName(STREAM_NAME, sa) < 0 )
    	die(-1, "error registering name: %s\n", STREAM_NAME);
	say("registered name: \n%s\n", STREAM_NAME);

	if (Xbind(acceptor, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		die(-3, "unable to bind to the dag\n");
	}

	Xlisten(acceptor, 5);

	while (1) {

		say("Xsock %4d waiting for a new connection.\n", acceptor);
		if ((sock = Xaccept(acceptor, NULL, 0)) < 0) {
			warn("Xsock %d accept failure! error = %d\n", acceptor, errno);
			// FIXME: should we die here instead or try and recover?
			continue;
		}

		say ("Xsock %4d new session\n", sock);

		pid_t pid = fork();

		if (pid == -1) {
			die(-1, "FORK FAILED\n");

		} else if (pid == 0) {
			process(sock, ctx);
			exit(0);

		} else {
			// FIXME: we need to close the socket in the main process or the file
			// descriptor limit will be hit. But if Xclose is called, the connection
			// is torn down in click as well keeping the child process from using it.
			// for now use a regular close to shut it here without affecting the child.
			close(sock);
		}
	}

	Xclose(acceptor);
	XSSL_CTX_free(ctx);
	free(SID);
}


int main(int argc, char *argv[])
{
	getConfig(argc, argv);

	say ("\n%s (%s): started\n", TITLE, VERSION);

	echo_stream();

	return 0;
}
