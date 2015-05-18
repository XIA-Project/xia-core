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
#include <netinet/in.h>
#include "Xsocket.h"
#include "dagaddr.hpp"

#define SID0 "SID:0f00000000000000000000000000000123456789"
#define DAG  "RE %s %s %s"
#define SNAME "tod_s.testbed.xia"

int main()
{
    int sock;
    char buf[XIA_MAXBUF];
	sockaddr_x client;
	time_t now;
	struct tm *t;
	int iface;

    // create a datagram socket
    if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
		printf("error: unable to create the listening socket.\n");
		exit(1);
	}

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID0, NULL, &ai) != 0) {
    	printf("error: unable to create source dag.");
		exit(1);
	}

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

    //Register this service name to the name server
    if (XregisterName(SNAME, sa) < 0) {
    	printf("error: unable to register name/dag combo");
		exit(1);
	}

    // bind to the DAG
    if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		printf("error: unable to bind to %s\n", SNAME);
		exit(1);
	}

    while (1) {
		// Use Xrecvmsg to get interface (click port) the connection came in on
		// lots of fun setup! maybe this can be streamlined a bit
		struct msghdr msg;
		struct iovec iov;
		struct in_pktinfo pi;

		struct cmsghdr *cmsg;
		struct in_pktinfo *pinfo;

		msg.msg_name = &client;
		msg.msg_namelen = sizeof(client);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		iov.iov_base = buf;
		iov.iov_len = XIA_MAXBUF;

		char cbuf[CMSG_SPACE(sizeof pi)];

		msg.msg_control = cbuf;
		msg.msg_controllen = sizeof(cbuf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(pi));

		msg.msg_controllen = cmsg->cmsg_len;

		if (Xrecvmsg(sock, &msg, 0) < 0) {
			printf("error receiving client request\n");
			// assume it's ok, and just keep listening
			continue;

		} else {

			// get the interface it came in on
			for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
					pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
					iface = pinfo->ipi_ifindex;
				}
			}
		}

		// we don't care what the client said, so we'll just ignore it

		now = time(NULL);
		t = gmtime(&now);
		strftime(buf, sizeof(buf), "%c %Z", t);
		
		Graph g(&client);
		printf("request on click port %hd from:\n%s\n", iface, g.dag_string().c_str());
			
		//Reply to client
		if (Xsendto(sock, buf, strlen(buf) + 1, 0, (struct sockaddr*)&client, sizeof(client)) < 0)
			printf("error sending time to the client\n");
    }

	Xclose(sock);
    return 0;
}

