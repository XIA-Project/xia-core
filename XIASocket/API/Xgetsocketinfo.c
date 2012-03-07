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

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <string.h>

int Xgetsocketinfo(int sockfd1, int sockfd2, struct Netinfo *info)
{
   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	char buf[MAXBUFLEN];
	struct sockaddr_in their_addr;
	socklen_t addr_len;

	if (getSocketType(sockfd1) == XSOCK_INVALID || 
		getSocketType(sockfd2) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}
	
	//Send a control packet 
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKCONTROLPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	// protobuf message
	xia::XSocketMsg xia_socket_msg;

	xia_socket_msg.set_type(xia::XGETSOCKETINFO);
	xia::X_Getsocketinfo_Msg *x_getsocketinfo_msg = xia_socket_msg.mutable_x_getsocketinfo();
	x_getsocketinfo_msg->set_id(sockfd2);

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	numbytes = sendto(sockfd1, p_buf.c_str(), p_buf.size(), 0,
					p->ai_addr, p->ai_addrlen);
	freeaddrinfo(servinfo);

	if (numbytes == -1) {
		perror("Xgetsocketinfo(): sendto failed");
		return(-1);
	}
 

        //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd1, buf, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("Xgetsocketinfo(): recvfrom");
                        return -1;
        }


	// HACK: due to protobuf null-terminated character handling issue
	std::string pp_buf;
	int inx;
	char tbuf[MAXBUFLEN];

	for(inx=0; inx< numbytes; inx++) {
		if (buf[inx] == '\0') {
			tbuf[inx] = '@'; // hack...
		} else {
			tbuf[inx] = buf[inx];
		}
	}
	tbuf[numbytes] = '\0';

	pp_buf = tbuf;

	for(inx=0; inx< numbytes; inx++) {
		if (pp_buf[inx] == '@') {
			pp_buf[inx] = '\0'; // hack...
		}
	}

	//protobuf message parsing
	//xia_socket_msg.Clear();

	// protobuf message
	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(pp_buf);

	if (xia_socket_msg1.type() == xia::XGETSOCKETINFO) {
		xia::X_Getsocketinfo_Msg *x_getsocketinfo_msg = xia_socket_msg1.mutable_x_getsocketinfo();
		info->port = x_getsocketinfo_msg->port(); 

		strcpy(info->src_path, x_getsocketinfo_msg->xiapath_src().c_str());
		strcpy(info->dst_path, x_getsocketinfo_msg->xiapath_dst().c_str());
		strcpy(info->status, x_getsocketinfo_msg->status().c_str());
		strcpy(info->protocol, x_getsocketinfo_msg->protocol().c_str());

 		return 1;
	}

	return -1; 

}

