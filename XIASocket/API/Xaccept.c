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

#include "Xsocket.h"
#include "Xinit.h"

int Xaccept(int sockfd)
{
	// Xaccept accepts the connection, creates new socket, and returns it.

   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	char buf[MAXBUFLEN];
	struct sockaddr_in my_addr, their_addr;
	socklen_t addr_len;
	int new_sockfd,tries;
	
        // Wait for connection from client
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("Xaccept: error 1");
                        return -1;
        }
        
        /*
	int src_port=ntohs(their_addr.sin_port);
	//printf ("src_port=%d \n", src_port);
	if (src_port!=atoi(CLICKACCEPTPORT)) {
		perror("Xaccept: error 2");
 		return -1;
	}	
	*/
	
	// Create new socket (this is a socket between API and Xtransport)
	int port;

	if ((new_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Xsocket accept listener: socket");
		return -1;
	}

	//If port is in use, try next port until success, or max ATTEMPTS reached
	srand((unsigned)time(0));
	rv=-1;
	for (tries=0;tries<ATTEMPTS;tries++)
	{
		int rn = rand();
		port=1025 + rn % (65535 - 1024); 
		my_addr.sin_family = PF_INET;
		my_addr.sin_addr.s_addr = inet_addr(MYADDRESS);
		my_addr.sin_port = htons(port);
		rv=bind(new_sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr));
		if (rv != -1)
			break;
	}
	if (rv == -1) {
		close(new_sockfd);
		perror("Xsocket accept listener: bind");
		return -1;
	}	
	//printf("my port=%d \n", port);
	
	// Do actual binding in Xtransport
	their_addr.sin_family = PF_INET;
	their_addr.sin_addr.s_addr = inet_addr(CLICKCONTROLADDRESS);
	their_addr.sin_port = htons(atoi(CLICKCONTROLPORT));	
	
	// protobuf message
	xia::XSocketMsg xia_socket_msg;

	xia_socket_msg.set_type(xia::XACCEPT);
	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);
		
	
	if ((numbytes = sendto(new_sockfd, p_buf.c_str(), p_buf.size(), 0,
					(const struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) {
		perror("Xaccept(): error 3");
		return(-1);
	}	
	 

	return new_sockfd;

}

