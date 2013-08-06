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
** @file Xrecvfrom.c
** @brief implements Xrecvfrom()
*/

/*
 * recvfrom like datagram receiving function for XIA
 * does not fill in DAG fields yet
 */

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

/*!
** @brief receives datagram data on an Xsocket.
**
** Xrecvfrom() retrieves data from an Xsocket of type XSOCK_DGRAM. Unlike the
** standard recvfrom API, it will not work with sockets of type XSOCK_STREAM.
**
** XrecvFrom() does not currently have a non-blocking mode, and will block
** until a data is available on sockfd. However, the standard socket API
** calls select and poll may be used with the Xsocket. Either function
** will deliver a readable event when a new connection is attempted and
** you may then call XrecvFrom() to get the data.
**
** NOTE: in cases where more data is received than specified by the caller,
** the excess data will be stored in the socket state structure and will
** be returned from there rather than from Click. Once the socket state
** is drained, requests will be sent through to Click again.
**
** @param sockfd The socket to receive with
** @param rbuf where to put the received data
** @param len maximum amount of data to receive. The amount of data
** returned may be less than len bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param addr if non-NULL, filled with the address of the sender on success
** @param addrlen contians the size of addr when called, replaced with the length
** of the received addr on return. addrlen MUST be set to the size of addr before
** calling Xrecvfrom() when addr is non-NULL. If addrlen is smaller than the length
** of the source DAG, the returned address is truncated and addrlen will contain length
** of the actual address.
**
** @returns number of bytes received
** @returns -1 on failure with errno set.
*/
int Xrecvfrom(int sockfd, void *rbuf, size_t len, int flags,
	struct sockaddr *addr, socklen_t *addrlen)
{
    int numbytes;

	if (flags != 0) {
		LOG("flags is not suppored at this time");
		errno = EINVAL;
		return -1;
	}

	if (!rbuf || (addr && !addrlen)) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}

	if (addr && *addrlen < sizeof(sockaddr_x)) {
		LOG("addr is not large enough");
		errno = EINVAL;
		return -1;
	}

	if (validateSocket(sockfd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", sockfd);
		return -1;
	}

	// see if we have bytes leftover from a previous Xrecv call
	if ((numbytes = getSocketData(sockfd, (char *)rbuf, len)) > 0) {
		// FIXME: we need to also have stashed away the sDAG and
		// return it as well
		*addrlen = 0;
		return numbytes;
	}


	xia::XSocketMsg xsm;
	unsigned seq;
	const char *payload;
	int paylen;
	xia::X_Recvfrom_Msg *xrm;

	// FIXME: do this the right way!
	// click replies immediately right now, so the recvfrom on the API socket
	// will always return immediately even if there is no data. Select won't
	// make a difference because of this. Changes need to be made in click so
	// that it doesn't fire back so rapidly.
	// inserting a short delay for now, but this still means polling far too
	// often.
	while (1) {
		//usleep(1000);
		sleep(1);

		xsm.set_type(xia::XRECVFROM);
		seq = seqNo(sockfd);
		xsm.set_sequence(seq);

		xrm = xsm.mutable_x_recvfrom();
		xrm->set_bytes_requested(len);

		if (click_send(sockfd, &xsm) < 0) {
			LOGF("Error talking to Click: %s", strerror(errno));
			return -1;
		}

		xsm.Clear();

		if ((numbytes = click_reply(sockfd, seq, &xsm)) < 0) {
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
			return -1;
		}
		xrm = xsm.mutable_x_recvfrom();
		payload = xrm->payload().c_str();
		paylen = xrm->bytes_returned();

		if (paylen != 0) {
			break;		
		}
	}

//	xrm = xsm.mutable_x_recvfrom();
//	const char *payload = xrm->payload().c_str();

	xia::X_Result_Msg *r = xsm.mutable_x_result();

	if (paylen < 0) {
		errno = r->err_code();
	
	} else if ((unsigned)paylen <= len) {
		memcpy(rbuf, payload, paylen);
	
	} else {
		// we got back more data than the caller requested
		// stash the extra away for subsequent Xrecv calls
		memcpy(rbuf, payload, len);
		paylen -= len;
		setSocketData(sockfd, payload + len, paylen);
		paylen = len;
	}

	if (addr) {
		Graph g(xrm->sender_dag().c_str());

		// FIXME: validate addr
		g.fill_sockaddr((sockaddr_x*)addr);
		*addrlen = sizeof(sockaddr_x);
	}


    return paylen;
}
