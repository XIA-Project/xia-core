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

int _xrecvfrom(int sockfd, void *rbuf, size_t len, int flags,
	sockaddr_x *addr, socklen_t *addrlen)
{
    int numbytes;
    char UDPbuf[MAXBUFLEN];

	if (flags != 0 && flags != MSG_PEEK) {
		LOGF("unsupported flag %d(s)", flags);
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

	// see if we have bytes leftover from a previous Xrecv call
	if ((numbytes = getSocketData(sockfd, (char *)rbuf, len)) > 0) {
		if (addr) {
			*addrlen = sizeof(sockaddr_x);
			memcpy(addr, dgramPeer(sockfd), sizeof(sockaddr_x));
		}
		else
			*addrlen = 0;
		return numbytes;
	}

	if ((numbytes = click_reply(sockfd, xia::XRECV, UDPbuf, sizeof(UDPbuf))) < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
		}
		return -1;
	}

	std::string str(UDPbuf, numbytes);
	xia::XSocketMsg xsm;

	xsm.ParseFromString(str);

	xia::X_Recv_Msg *msg = xsm.mutable_x_recv();
	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();

	if (paylen <= len)
		memcpy(rbuf, payload, paylen);
	else {
		// we got back more data than the caller requested
		// stash the extra away for subsequent Xrecv calls
		memcpy(rbuf, payload, len);
		paylen -= len;
		setSocketData(sockfd, payload + len, paylen);
		paylen = len;
	}

	if (addr || (flags & MSG_PEEK)) {
		Graph g(msg->dag().c_str());

		if (addr) {
			// FIXME: validate addr
			g.fill_sockaddr((sockaddr_x*)addr);
			*addrlen = sizeof(sockaddr_x);
		}

		// user peeked, so save all of the data
		if (flags & MSG_PEEK) {
			sockaddr_x sa;

			if (!addr) {
				addr = &sa;
				g.fill_sockaddr((sockaddr_x*)addr);

			}
			printf("len: %d p: %p\n", msg->payload().size(), payload);
			setSocketData(sockfd, payload, msg->payload().size());
			setPeer(sockfd, addr);
		}
	}

    return paylen;

}

int _xrecvfromconn(int sockfd, void *rbuf, size_t len, int flags)
{
	int rc;
	sockaddr_x sa;
	socklen_t addrlen;
	Graph g(dgramPeer(sockfd));

	if (g.num_nodes() == 0) {
		errno = EHOSTUNREACH;
		return -1;
	}

	Node nPeer = g.get_final_intent();

	while (1) {

		addrlen = sizeof(sa);
		rc = _xrecvfrom(sockfd, rbuf, len, flags, &sa, &addrlen);

		if (rc < 0)
			return rc;

		// check to see if addr and the stored addr for the connection are the same
		g.from_sockaddr(&sa);

		if (g.get_final_intent() == nPeer)
			break;

		// packet came from a different peer, just discard it and try again
		LOGF("discarding packet from unconnected peer: %s", g.dag_string().c_str());
	}

	return rc;
}


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
	if (validateSocket(sockfd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", sockfd);
		return -1;
	}

	if (connState(sockfd) == CONNECTED) {
		LOGF("socket %d is connected, use Xrecv instead!", sockfd);
		errno = EISCONN;
		return -1;
	}

	return _xrecvfrom(sockfd, rbuf, len, flags, (sockaddr_x *)addr, addrlen);
}
