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
* GetCID request
*/

#include "Xsocket.h"
#include "Xinit.h"

int XgetCID(int sockfd, char* cDAG, size_t dlen)
{
        
	//char buffer[MAXBUFLEN];
	//struct sockaddr_in their_addr;
	//socklen_t addr_len;

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	const char *buf="CID request";//Maybe send more useful information here.
	int numCIDs = 1;

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

	xia_socket_msg.set_type(xia::XGETCID);

	xia::X_Getcid_Msg *x_getcid_msg = xia_socket_msg.mutable_x_getcid();
  
  	x_getcid_msg->set_numcids(numCIDs);
	x_getcid_msg->set_cdaglist(cDAG);
	x_getcid_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0, p->ai_addr, p->ai_addrlen);
	freeaddrinfo(servinfo);

	if (numbytes == -1) {
		perror("Xgetcid(): getcid failed");
		return(-1);
	}

     /*	 
       //Process the reply
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buffer, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("Xgetcid(): recvfrom");
                        return -1;
        }

	//protobuf message parsing
	xia_socket_msg.ParseFromString(buffer);

	if (xia_socket_msg.type() == xia::XSOCKET_SENDTO) {

 		return numbytes;
	}
        return -1; 
      */

	return numbytes;    
    
}




