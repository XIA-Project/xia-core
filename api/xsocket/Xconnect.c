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
** @file Xconnect.c
** @brief implements Xconnect()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Initiate a connection on an Xsocket of type XSOCK_STREAM
**
** The Xconnect() call connects the socket referred to by sockfd to the
** SID specified by dDAG. It is only valid for use with sockets created
** with the XSOCK_STREAM Xsocket type.
**
** @note Xconnect() differs from the standard connect API in that it does
** not currently support use with Xsockets created with the XSOCK_DGRAM
** socket type.
**
** @param sockfd	The control socket
** @param dDAG	The DAG (SID) of the remote service to connect to.
**
** @returns 0 on success
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard connect call.
*/
int Xconnect(int sockfd, const char* dDAG)
{
	int rc;

	int numbytes;
	char buf[MAXBUFLEN];
	struct sockaddr_in their_addr;
	socklen_t addr_len;
	
	// FIXME: should we be validating the destination DAG?

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xconnect is only valid with stream sockets.");
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XCONNECT);

	xia::X_Connect_Msg *x_connect_msg = xsm.mutable_x_connect();
	x_connect_msg->set_ddag(dDAG);

	// In Xtransport: send SYN to destination server
	if ((rc = click_control(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// Waiting for SYNACK from destination server

	// FIXME: make this use protobufs
#if 1	
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	if (strcmp(buf, "^Connection-failed^") == 0) {
		errno = ECONNREFUSED;
		LOG("Connection Failed");
		return -1;	    
	} else {
		setConnected(sockfd, 1);
		return 0; 
	}
#endif
}

