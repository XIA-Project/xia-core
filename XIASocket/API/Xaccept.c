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
** @brief implements Xaccept()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Accept a conection from a remote Xsocket
**
** The Xaccept system call is is only valid with Xsockets created with
** the XSOCK_STREAM transport type. It accepts the first available connection
** request for the listening socket, sockfd, creates a new connected socket, 
** and returns a new Xsocket descriptor referring to that socket. The newly 
** created socket is not in the listening state. The original socket 
** sockfd is unaffected by this call.
**
** Xaccept does not currently have a non-blocking mode, and will block
** until a connection is made. However, the standard socket API calls select
** and poll may be used with the Xsocket. Either function will deliver a
** readable event when a new connection is attempted and you may then call
** Xaccept() to get a socket for that connection. 
**
** @note Unlike standard sockets, there is currently no Xlisten function. 
** Callers must create the listening socket by calling Xsocket with the 
** XSOCK_STREAM transport_type and bind it to a source DAG with Xbind(). XAccept
** may then be called to wait for connections.
**
** @param sockfd	an Xsocket() previously created with the XSOCK_STREAM type,
** and bound to a local DAG with Xbind()
**
** @returns a non-negative integer that is the new Xsocket id
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard accept call.
*/
int Xaccept(int sockfd)
{
	// Xaccept accepts the connection, creates new socket, and returns it.

	int numbytes;
	char buf[MAXBUFLEN];
	struct sockaddr_in addr;
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	int new_sockfd;
	xia::XSocketCallType type;

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xaccept is only valid with stream sockets.");
		return -1;
	}
	
	// Wait for connection from client
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		LOGF("Error reading from socket (%d): %s", sockfd, 
				strerror(errno));
		return -1;
	}
        
	// Create new socket (this is a socket between API and Xtransport)

	if ((new_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("Error creating new socket: %s", strerror(errno));
		return -1;
	}

	// bind to an unused random port number
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr(MYADDRESS);
	addr.sin_port = 0;

	if (bind(new_sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(new_sockfd);
		LOGF("Error binding new socket to local port: %s", strerror(errno));
		return -1;
	}	
	
	// Do actual binding in Xtransport

	xia::XSocketMsg xia_socket_msg;
	xia_socket_msg.set_type(xia::XACCEPT);

	if (click_control(new_sockfd, &xia_socket_msg) < 0) {
		close(new_sockfd);
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if (click_reply2(new_sockfd, &type) < 0) {
		close(new_sockfd);
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	allocSocketState(new_sockfd, XSOCK_STREAM);
	setConnected(new_sockfd, 1);

	return new_sockfd;
}
