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
#include "Xkeys.h"
#include "dagaddr.hpp"

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
** @param addr	The address (SID) of the remote service to connect to.
** @param addrlen The length of addr
**
** @returns 0 on success
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard connect call.
*/
int Xconnect(int sockfd, const sockaddr *addr, socklen_t addrlen)
{
	int rc;

	int numbytes;
	char src_SID[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	char buf[MAXBUFLEN];
	struct sockaddr_in their_addr;
	struct addrinfo *ai;
	socklen_t addr_len;

	printf("Xconnect: called\n");
	// we can't count on addrlen being set correctly if we are being called via
	// the wrapper functions as the original source program doesn't know that
	// a sockaddr_x is larger than a sockaddr	
//	if (!addr || addrlen < sizeof(sockaddr_x)) {
	if (!addr) {
		errno = EINVAL;
		return -1;
	}
	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xconnect is only valid with stream sockets.");
		return -1;
	}

	Graph g((sockaddr_x*)addr);
	if (g.num_nodes() <= 0) {
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XCONNECT);

	xia::X_Connect_Msg *x_connect_msg = xsm.mutable_x_connect();
	x_connect_msg->set_ddag(g.dag_string().c_str());
	// Assign a SID with corresponding keys (unless assigned by bind already)
	if(!isSIDAssigned(sockfd)) {
		printf("Xconnect: generating key pair for random SID\n");
		if(XmakeNewSID(src_SID, sizeof(src_SID))) {
			LOG("Unable to create a new SID with key pair");
			printf("Xconnect: Unable to create a new SID with key pair");
			return -1;
		}
        LOGF("Generated SID:%s:", src_SID);
        printf("Generated SID:%s:\n", src_SID);

		// Convert SID to a default DAG
		if(Xgetaddrinfo(NULL, src_SID, NULL, &ai)) {
			LOGF("Unable to convert %s to default DAG", src_SID);
			printf("Xconnect: Unable to convert %s to default DAG\n", src_SID);
			return -1;
		}
		sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
		Graph src_g(sa);
		// Include the source DAG in message to xtransport
		printf("Xconnect: random DAG:%s:\n", src_g.dag_string().c_str());
		x_connect_msg->set_sdag(src_g.dag_string().c_str());
		Xfreeaddrinfo(ai);
		setSIDAssigned(sockfd);
		setTempSID(sockfd, src_SID);
	}
	// In Xtransport: send SYN to destination server
	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}
	// Waiting for SYNACK from destination server

	// FIXME: make this use protobufs
#if 1	
	addr_len = sizeof their_addr;
	setWrapped(sockfd, 1);
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
	setWrapped(sockfd, 0);
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}
	setWrapped(sockfd, 0);

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

