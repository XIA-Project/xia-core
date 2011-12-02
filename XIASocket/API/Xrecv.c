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
* recv like datagram receiving function for XIA
* IMPORTANT: works for datagrams only
*/

#include "Xsocket.h"
#include "Xinit.h"

int Xrecv(int sockfd, void *buf, size_t len, int flags)
{
	struct addrinfo hints;
	int numbytes;
	socklen_t addr_len;
	char UDPbuf[MAXBUFLEN];
	struct sockaddr_in their_addr;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	//printf("(%s) recv \n", _conf.api_addr);

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("Xrecv: recvfrom");
		return -1;
	}
	int src_port=ntohs(their_addr.sin_port);
	
	//Check if it's a control message
	while(src_port==atoi(CLICKCONTROLPORT))
	{
		//Do what is necessary, maybe close socket
		if(strcmp((char *)buf,"close")==0)
		{
			Xclose(sockfd);
			return -1;
		}
		else
		{ 
			if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) 
			{
				perror("Xrecv: recvfrom");
				return -1;
		   	}
		   	

		}
	}
	
	/*short int paylen=0;
	memcpy (&paylen, UDPbuf+2,2);
	paylen=ntohs(paylen);
	int offset=numbytes-paylen;
	strncpy(buf, UDPbuf+offset, paylen);
	
	return paylen;*/
	
	short int paylen=0,i=0;
	char* tmpbuf=(char*)UDPbuf;
	while(tmpbuf[i]!='^')
		i++;
	paylen=numbytes-i-1;
	//memcpy (&paylen, UDPbuf+2,2);
	//paylen=ntohs(paylen);
	int offset=i+1;
	memcpy(buf, UDPbuf+offset, paylen);
	//strncpy(sDAG, UDPbuf, i);
	
	return paylen;
}
