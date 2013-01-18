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

/* xtods - a stupidly simple gettimeofday server that returns the time in a string the result using localtime */

#include <time.h>
#include "Xsocket.h"

#define SID0 "SID:0f00000000000000000000000000000123456789"
#define DAG  "RE %s %s %s"
#define SNAME "tod_s.testbed.xia"

int main()
{
    int sock;
    size_t len;
	char dag[256];
    char buf[XIA_MAXBUF];
	char client[256];
	time_t now;
	struct tm *t;

	char ad[128], hid[128], fid[128];

    // create a datagram socket
    if ((sock = Xsocket(XSOCK_DGRAM)) < 0) {
		printf("error: unable to create the listening socket.\n");
		exit(1);
	}

    // read the localhost AD and HID
    if ( XreadLocalHostAddr(sock, ad, sizeof(ad), hid, sizeof(hid), fid, sizeof(fid)) < 0 ) {
    	printf("error: Reading localhost address");
		exit(1);
	}

    // make the DAG we will listen on
    sprintf(dag, DAG, ad, hid, SID0); 

    //Register this service name to the name server
    if (XregisterName(SNAME, dag) < 0) {
    	printf("error: unable to register name/dag combo");
		exit(1);
	}

    // bind to the DAG
    if (Xbind(sock, dag) < 0) {
		Xclose(sock);
		printf("error: unable to bind to the dag (%s)\n", dag);
		exit(1);
	}

    while (1) {
		len = sizeof(client);
		if (Xrecvfrom(sock, buf, sizeof(buf), 0, client, &len) < 0) {
			printf("error receiving client request\n");
			// assume it's ok, and just keep listening
			continue;
		}

		// we don't care what the client said, so we'll just ignore it

		// make sure the client's dag is null terminated
		if (len == sizeof(client))
			client[len - 1] = 0;

		now = time(NULL);
		t = gmtime(&now);
		strftime(buf, sizeof(buf), "%c %Z", t);
		printf("request from: (%s)\n", client);
			
		//Reply to client
		if (Xsendto(sock, buf, strlen(buf) + 1, 0, client, strlen(client)) < 0)
			printf("error sending time to the client\n");
    }

	Xclose(sock);
    return 0;
}

