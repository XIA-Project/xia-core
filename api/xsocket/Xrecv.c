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
** @file Xrecv.c
** @brief Xrecv(), Xrecvfrom() - receive a message from a socket
*/

#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
/*! \endcond */

/*!
** @brief Receive data from an Xsocket
**
** The Xrecv() call is used to receive messages from a connected Xsocket.
**
** If no data is available at the socket, Xrecv() will wait for a message to arrive, unless the
** socket is nonblocking (see Xfcntl()), in which case the value -1 is returned and errno
** is set to EAGAIN or EWOULDBLOCK. This call will return any data available, up to the
** requested amount, rather than waiting for receipt of the full amount requested.
**
** An application can use Xselect() or Xpoll(), or to determine when more data arrives on a socket.
**
** @param sockfd The socket to receive with
** @param rbuf where to put the received data
** @param len maximum amount of data to receive. the amount of data
** returned may be less than len bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard send socket call).
**
** @returns the number of bytes returned, which may be less than the number
** requested by the caller
** @returns -1 on failure with errno set to an error compatible with the standard
** recv call.
*/
int Xrecv(int sockfd, void *rbuf, size_t len, int flags)
{
	int numbytes;
	int iface = 0;

	int stype = getSocketType(sockfd);
	if (stype == SOCK_DGRAM) {
		return _xrecvfromconn(sockfd, rbuf, len, flags, &iface);

	} else if (stype != SOCK_STREAM) {
		LOGF("Socket %d must be a stream or datagram socket", sockfd);
		errno = EOPNOTSUPP;
		return -1;
	}

	if (len == 0)
		return 0;

	// FIXME: this is overkill, figure out how much room we really need to reserve
	// make sure there's enough room for the protobuf
	//size_t max_buf = api_mtu() - 1000;
	//if (len > max_buf)
	//	len = max_buf;
	len = MIN(len, XmaxPayload());


	if (!rbuf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (getConnState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}



	xia::XSocketMsg xsm;
	xsm.set_type(xia::XRECV);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Recv_Msg *xrm = xsm.mutable_x_recv();
	xrm->set_bytes_requested(len);
	xrm->set_flags(flags);

	if (click_send(sockfd, &xsm) < 0) {
		if (isBlocking(sockfd) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}

	xsm.Clear();
	if ((numbytes = click_reply(sockfd, seq, &xsm)) < 0) {
		if (isBlocking(sockfd) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
		}
		return -1;
	} else if (numbytes == 0) {
		// the socket has closed gracefully on the other end
		LOG("The peer closed the connection");
		return 0;
	}

	xrm = xsm.mutable_x_recv();
	const char *payload = xrm->payload().c_str();

	xia::X_Result_Msg *r = xsm.mutable_x_result();
	int paylen = r->return_code();

	if (paylen < 0) {
		errno = r->err_code();

	} else if ((size_t)paylen <= len) {
		memcpy(rbuf, payload, paylen);

	} else {
		// THIS SHOULD NOT HAPPEN ANYMORE
		// we got back more data than the caller requested

		memcpy(rbuf, payload, len);
		paylen = len;
	}
	return paylen;
}

/*!
** @brief receives datagram data on an Xsocket
**
** Xrecvfrom() retrieves data from an Xsocket of type XSOCK_DGRAM. Unlike the
** standard recvfrom API, it will not work with sockets of type XSOCK_STREAM.
**
** If no data is available at the socket, Xrecvfrom() will wait for a message to arrive, unless the
** socket is nonblocking (see Xfcntl()), in which case the value -1 is returned and errno
** is set to EAGAIN or EWOULDBLOCK. This return any data available, up to the
** requested amount, rather than  waiting for receipt of the full amount requested.
**
** An application can use Xselect() or Xpoll(), or to determine when more data arrives on a socket.
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
** @returns number of bytes returned
** @returns -1 on failure with errno set.
*/
int Xrecvfrom(int sockfd, void *rbuf, size_t len, int flags,
	struct sockaddr *addr, socklen_t *addrlen)
{
	int iface = 0;

	if (validateSocket(sockfd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", sockfd);
		return -1;
	}

	if (getConnState(sockfd) == CONNECTED) {
		LOGF("socket %d is connected, use Xrecv instead!", sockfd);
		errno = EISCONN;
		return -1;
	}

	return _xrecvfrom(sockfd, rbuf, len, flags, (sockaddr_x *)addr, addrlen, &iface);
}


int _xrecvfrom(int sockfd, void *rbuf, size_t len, int flags,
	sockaddr_x *addr, socklen_t *addrlen, int *iface)
{
	int numbytes;

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

	xia::XSocketMsg xsm;
	unsigned seq;
	const char *payload;
	int paylen;
	xia::X_Recvfrom_Msg *xrm;

	xsm.set_type(xia::XRECVFROM);
	seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xrm = xsm.mutable_x_recvfrom();
	xrm->set_bytes_requested(len);
	xrm->set_flags(flags);

	if (click_send(sockfd, &xsm) < 0) {
		if (isBlocking(sockfd) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}

	xsm.Clear();

	if ((numbytes = click_reply(sockfd, seq, &xsm)) < 0) {
		if (isBlocking(sockfd) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
		}
		return -1;
	}
	xrm = xsm.mutable_x_recvfrom();
	payload = xrm->payload().c_str();
	paylen = xrm->bytes_returned();

	*iface = xrm->interface_id();

	if ((unsigned)paylen <= len) {
		memcpy(rbuf, payload, paylen);

	} else {
		// TRUNCATE and discard the extra tail
		memcpy(rbuf, payload, len);
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

int _xrecvfromconn(int sockfd, void *rbuf, size_t len, int flags, int *iface)
{

	int rc;
	sockaddr_x sa;
	socklen_t addrlen;
	Graph g(dgramPeer(sockfd));

	if (g.num_nodes() == 0) {
		errno = EHOSTUNREACH;
		return -1;
	}

	if (len == 0)
		return 0;

	if (!rbuf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (getConnState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	Node nPeer = g.get_final_intent();

	// the loop discards packets that are received on the socket and are not from our connected peer
	while (1) {

		addrlen = sizeof(sa);
		rc = _xrecvfrom(sockfd, rbuf, len, flags, &sa, &addrlen, iface);

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
