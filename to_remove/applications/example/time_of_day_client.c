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

/* xtodc - a stupidly simple Xsocket time of day client */

#include "Xsocket.h"

#define SNAME "tod_s.testbed.xia"

int main()
{
    int sock;
	sockaddr_x sa;
	socklen_t slen;
	int rc;
	char buf[2048];

    // create a datagram socket
    if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
		printf("error: unable to create the listening socket.\n");
		exit(1);
	}

    // lookup the xia service 
	slen = sizeof(sa);
    if (XgetDAGbyName(SNAME, &sa, &slen) < 0) {
		printf("unable to locate: %s\n", SNAME);
		exit(1);
	}

	// the server doesn't care what we send in the request
	if ((rc = Xsendto(sock, "hello", strlen("hello"), 0, (struct sockaddr*)&sa, slen)) >= 0) {

		if (( rc = Xrecvfrom(sock, buf, sizeof(buf), 0, NULL, NULL)) < 0) {
			printf("error getting response from the time server\n");
		} else {
			printf("%s\n", buf);
		}

	} else {
		printf("error sending request to the time server\n");
	}

	Xclose(sock);
	return 0;
}

