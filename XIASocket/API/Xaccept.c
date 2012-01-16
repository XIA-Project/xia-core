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
/*!
** @file Xaccept.c
** @brief implements Xaccept
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief The Xaccept system call is is only valid with Xsockets created with
** the XSOCK_STREAM tranport type. It accepts the first availble connection
** request for the listening socket, sockfd, creates a new connected socket, 
** and returns a new Xsocket descriptor referring to that socket. The newly 
** created socket is not in the listening state. The original socket 
** sockfd is unaffected by this call.

** @note Unlike standard sockets, there is currently no Xlisten function. 
** Callers must create the listening socet by calling Xsocket with the 
** XSOCK_STREAM transport_type and bind it to a source DAG with Xbind. XAccept
** may then be called to wait for connections.
**
** @param sockfd	The control socket
**
** @returns a non-negative integer that is the new Xsocket id
** @returns -1 on error with errno set
*/
int Xaccept(int sockfd)
{
	// Xaccept accepts the connection, creates new socket, and returns it.

	int rv;
	int numbytes;
	char buf[MAXBUFLEN];
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	int new_sockfd;
	xia::XSocketCallType type;
	
	// Wait for connection from client
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("Xaccept: error 1");
		return -1;
	}
        
	// Create new socket (this is a socket between API and Xtransport)

	if ((new_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Xsocket listener: socket");
		return -1;
	}

	rv = bind_to_random_port(new_sockfd);
	if (rv == -1) {
		close(new_sockfd);
		perror("Xsocket listener: bind");
		return -1;
	}	
	
	// Do actual binding in Xtransport

	xia::XSocketMsg xia_socket_msg;
	xia_socket_msg.set_type(xia::XACCEPT);

	click_control(new_sockfd, &xia_socket_msg);

	if (click_reply2(new_sockfd, &type) < 0) {
		close(new_sockfd);
		perror("XAccept failure: ");
		return -1;
	}

	return new_sockfd;
}
