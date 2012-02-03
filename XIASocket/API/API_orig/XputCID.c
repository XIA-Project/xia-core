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

/*
* sendto like datagram sending function for XIA
*/

#include "Xsocket.h"
#include "Xinit.h"

int XputCID(int sockfd, const void *buf, size_t len, int /*flags*/,
		char* sDAG, size_t /*dlen*/)
{


	/* === New version
	 * Now, the DAG and data are contained in the google protobuffer message (encapsulated within UDP),
	 * then passed to the Click UDP.
	 */

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	
	//char buffer[MAXBUFLEN];
	//struct sockaddr_in their_addr;
	//socklen_t addr_len;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKDATAPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	// protobuf message
	xia::XSocketMsg xia_socket_msg;

	xia_socket_msg.set_type(xia::XPUTCID);

	xia::X_Putcid_Msg *x_putcid_msg = xia_socket_msg.mutable_x_putcid();
	x_putcid_msg->set_sdag(sDAG);
	//printf("PUTCID len %d\n", len);
	x_putcid_msg->set_payload((const char*)buf, len);


	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0, p->ai_addr, p->ai_addrlen);
	freeaddrinfo(servinfo);

	if (numbytes == -1) {
		perror("XputCID(): sendto failed");
		return(-1);
	}

    /*
        //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("XputCID(): recvfrom");
                        return -1;
        }

	//protobuf message parsing
	xia_socket_msg.ParseFromString(buffer);

	if (xia_socket_msg.type() == xia::XSOCKET_PUTCID) {

 		return numbytes;
	}

        return -1; 
        return -1; 
      */

	return numbytes;

}
