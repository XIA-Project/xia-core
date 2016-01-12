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

int _connDgram(int sockfd, const sockaddr *addr, socklen_t addrlen)
{
	UNUSED(addrlen);
	int rc = 0;

	if (addr->sa_family == AF_UNSPEC) {

		// go back to allowing connections to/from any peer
		connectDgram(sockfd, NULL);

	} else if (addr->sa_family == AF_XIA) {
		// validate addr
		Graph g((sockaddr_x *)addr);

		if (g.num_nodes() == 0) {
			rc = -1;
			errno = EHOSTUNREACH;

		// FIXME: can we verify addrlen here?

		} else {
			connectDgram(sockfd, (sockaddr_x *)addr);
		}
	} else {
		rc = -1;
		errno = EAFNOSUPPORT;
	}

	return rc;
}

int _connStream(int sockfd, const sockaddr *addr, socklen_t addrlen)
{
	UNUSED(addrlen);
	int rc;
	char src_SID[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	struct addrinfo *ai;

	if (addr->sa_family != AF_XIA) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	// FIXME: check addrlen here. check against wrapper, there were problems with it not setting length correctly

	Graph g((sockaddr_x*)addr);
	if (g.num_nodes() <= 0) {
		errno = EADDRNOTAVAIL;
		return -1;
	}

	int state = getConnState(sockfd);
	if (state == CONNECTED) {
		errno = EALREADY;
		return -1;
	} else if (state == CONNECTING) {
		errno = EINPROGRESS;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XCONNECT);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Connect_Msg *x_connect_msg = xsm.mutable_x_connect();
	x_connect_msg->set_ddag(g.dag_string().c_str());

	// Assign a SID with corresponding keys (unless assigned by bind already)
	if(!isSIDAssigned(sockfd)) {
		LOG("Xconnect: generating key pair for random SID\n");
		if(XmakeNewSID(src_SID, sizeof(src_SID))) {
			LOG("Unable to create a new SID with key pair");
			return -1;
		}
        LOGF("Generated SID:%s:", src_SID);

		// Convert SID to a default DAG
		if(Xgetaddrinfo(NULL, src_SID, NULL, &ai)) {
			LOGF("Unable to convert %s to default DAG", src_SID);
			return -1;
		}

		sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
		Graph src_g(sa);

		// Include the source DAG in message to xtransport
		LOGF("Xconnect: random DAG:%s:\n", src_g.dag_string().c_str());
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

	setConnState(sockfd, CONNECTING);

	rc = click_status(sockfd, seq);
	if (rc == -1) {
		if (errno != EINPROGRESS) {
			setConnState(sockfd, UNCONNECTED);
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
			return -1;
		} else if (!isBlocking(sockfd)) {
			// we're non blocking, tell app to go about its business
			return -1;
		}
	} else {
		// something bad happened! we shouldn't get a success code here
		LOG("this success is an error!\n");
	}

	// Waiting for SYNACK from destination server
    int clickrc = click_reply(sockfd, 0, &xsm);
    if (clickrc < 0 || xsm.x_connect().status() != xia::X_Connect_Msg::XCONNECTED) {
        setConnState(sockfd, UNCONNECTED);
        LOGF("Xconnect failed: %s", strerror(errno));
        return -1;
    }

	setConnState(sockfd, CONNECTED);
	return 0;
}

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
	int stype = getSocketType(sockfd);
	int rc = -1;

	if (!addr) {
		errno = EINVAL;
		rc = -1;

	} else if (stype == SOCK_DGRAM) {
		rc = _connDgram(sockfd, addr, addrlen);

	} else if (stype == SOCK_STREAM) {
		rc = _connStream(sockfd, addr, addrlen);

	} else {
		errno = EBADF;
		LOG("Invalid socket type, only SOCK_STREAM and SOCK_DGRAM allowed");
		rc = -1;
	}

	return rc;
}

