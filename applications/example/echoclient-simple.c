/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**	http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*simple interactive echo client using Xsockets */

#include <stdio.h>
#include <stdlib.h>
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Echo Client"

#define STREAM_NAME "www_s.stream_echo.aaa.xia"
#define DGRAM_NAME  "www_s.dgram_echo.aaa.xia"

void help()
{
	printf("usage: xecho [-ds]\n");
	printf("where:\n");
	printf("  -d : use a datagram socket\n");
	printf("  -s : use a stream socket (default)\n");
	exit(1);
}

void echo_dgram()
{
	int sock;
	char *dag;
	char sdag[512];
	char buf[2048];
	char reply[2048];
	int ns, nr;
	size_t len;
	
	if ((sock = Xsocket(XSOCK_DGRAM)) < 0) {
		printf("error creating socket\n");
		exit(1);
	}

    // lookup the xia service 
    if (!(dag = XgetDAGbyName(DGRAM_NAME))) {
		printf("unable to locate: %s\n", DGRAM_NAME);
		exit(1);
	}

	while(1) {
		printf("\nPlease enter the message (blank line to exit):\n");
		fgets(buf, sizeof(buf), stdin);
		if ((ns = strlen(buf)) <= 1)
			break;

		if (Xsendto(sock, buf, ns, 0, dag, strlen(dag)) < 0) {
			printf("error sending message\n");
			break;
		}

		len =  sizeof(sdag);
		if ((nr = Xrecvfrom(sock, reply, sizeof(reply), 0, sdag, &len)) < 0) {
			printf("error receiving message\n");
			break;
		}

		reply[nr] = 0;
		if (ns != nr)
			printf("warning: sent %d characters, received %d\n", ns, nr);
		printf("%s", reply);
	}

	Xclose(sock);
}

void echo_stream()
{
	int sock;
	char *dag;
	char buf[2048];
	char reply[2048];
	int ns, nr;

	if ((sock = Xsocket(XSOCK_STREAM)) < 0) {
		printf("error creating socket\n");
		exit(1);
	}

    // lookup the xia service 
    if (!(dag = XgetDAGbyName(STREAM_NAME))) {
		printf("unable to locate: %s\n", STREAM_NAME);
		exit(1);
	}

	if (Xconnect(sock, dag) < 0) {
		printf("can't connect to %s\n", dag);
		Xclose(sock);
		exit(1);
	}

	while(1) {
		printf("\nPlease enter the message (blank line to exit):\n");
		fgets(buf, sizeof(buf), stdin);
		if ((ns = strlen(buf)) <= 1)
			break;
		
		if (Xsend(sock, buf, ns, 0) < 0) {
			printf("error sending message\n");
			break;
		}

		if ((nr = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
			printf("error receiving message\n");
			break;
		}

		reply[nr] = 0;
		if (ns != nr)
			printf("warning: sent %d characters, received %d\n", ns, nr);
		printf("%s", reply);
	}

	Xclose(sock);
}

int main(int argc, char **argv)
{
	int mode = 1;

	printf("%s %s\n", TITLE, VERSION);

	// determine if we should use a stream or a datagram connection
	if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0)
			mode = 0;
		else if(strcmp(argv[1], "-s") == 0)
			mode = 1;
		else
			help();
	}
	else if (argc > 2)
		help();

	printf("running in %s mode\n", mode == 0 ? "datagram" : "stream");

	if (mode == 0)
		echo_dgram();
	else
		echo_stream();
}

