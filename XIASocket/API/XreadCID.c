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
* ReadCID
*/

#include "Xsocket.h"
#include "Xinit.h"

// Called after XgetCID(), it reads the content of the requested CID (specified as cDAG) into buf
// Return value: number of bytes, -1: failed 

int XreadCID(int sockfd, void *rbuf, size_t len, int flags, char * cDAG, size_t dlen)
{
	char buffer[2048];
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	char statusbuf[2048];
	char UDPbuf[MAXBUFLEN];
	
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
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

	xia_socket_msg.set_type(xia::XREADCID);

	xia::X_Readcid_Msg *x_readcid_msg = xia_socket_msg.mutable_x_readcid();
  
  	x_readcid_msg->set_numcids(numCIDs);
	x_readcid_msg->set_cdaglist(cDAG);

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	numbytes = sendto(sockfd, p_buf.c_str(), p_buf.size(), 0, p->ai_addr, p->ai_addrlen);
	freeaddrinfo(servinfo);

	if (numbytes == -1) {
		perror("XreadCID(): XreadCID failed");
		return(-1);
	}



     	 
       //Process the reply
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , 0,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("XreadCID(): recvfrom");
		return -1;
	}
	
      	short int paylen=0,i=0;
	char* tmpbuf=(char*)UDPbuf;
	while(tmpbuf[i]!='^')
		i++;
	paylen=numbytes-i-1;
	//memcpy (&paylen, UDPbuf+2,2);
	//paylen=ntohs(paylen);
	int offset=i+1;
	memcpy(rbuf, UDPbuf+offset, paylen);
	//strncpy(sDAG, UDPbuf, i);
	
	return paylen;
    
}




