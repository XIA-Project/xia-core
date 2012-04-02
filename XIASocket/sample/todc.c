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

#define HID0 "HID:0000000000000000000000000000000000000000"
#define AD0  "AD:1000000000000000000000000000000000000000"
#define SID0 "SID:0f00000000000000000000000000000123456789"
#define DAG  "RE %s %s %s"

int main()
{
    int sock;
	int rc;
	size_t len;
	char buf[2048];
	char dag[512];
	char server[512];

    // create a datagram socket
    if ((sock = Xsocket(XSOCK_DGRAM)) < 0) {
		printf("error: unable to create the listening socket.\n");
		exit(1);
	}

    // make the DAG of the time server
    sprintf(dag, DAG, AD0, HID0, SID0); 

	// the server doesn't care what we send in the request
	strcpy(buf, "hello");

	if ((rc = Xsendto(sock, buf, strlen(buf), 0, dag, strlen(dag) + 1)) >= 0) {

		len = sizeof(server);
		if (( rc = Xrecvfrom(sock, buf, sizeof(buf), 0, server, &len)) < 0) {
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

