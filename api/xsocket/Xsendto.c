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
** @file Xsendto.c
** @brief implements Xsendto()
*/
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>
#include "dagaddr.hpp"


int _xsendto(int sockfd, const void *buf, size_t len, int flags,
	const sockaddr_x *addr, socklen_t addrlen)
{
	int rc;

	if (len == 0)
		return 0;

	if (addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	if (flags != 0) {
		LOG("the flags parameter is not currently supported");
		errno = EINVAL;
		return -1;
	}

	if (!buf || !addr) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}

	// if buf is too big, send only what we can
	if (len > XIA_MAXBUF) {
		LOGF("truncating... requested size (%d) is larger than XIA_MAXBUF (%d)\n",
				len, XIA_MAXBUF);
		len = XIA_MAXBUF;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSENDTO);

	xia::X_Sendto_Msg *x_sendto_msg = xsm.mutable_x_sendto();

	// FIXME: validate addr
	Graph g((sockaddr_x*)addr);
	std::string s = g.dag_string();

	x_sendto_msg->set_ddag(s.c_str());
	x_sendto_msg->set_payload((const char*)buf, len);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		if (!WOULDBLOCK()) {
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}

	// because we don't have queueing or seperate control and data sockets, we
	// can't get status back reliably on a datagram socket as multiple peers
	// could be talking to it at the same time and the control messages can get
	// mixed up with the data packets. So just assume that all went well and tell
	// the caller we sent the data

	return len;
}

/*!
** @brief Sends a datagram message on an Xsocket
**
** Xsendto sends a datagram to the specified address. The final intent of
** the address should be a valid SID.
**
** Unlike a standard socket, Xsendto() is only valid on Xsockets of
** type XSOCK_DGRAM.
**
** If the buffer is too large, Xsendto() will truncate the message and
** send what it can. This is different from the standard sendto which returns
** an error.
**
** @param sockfd The socket to send the data on
** @param buf the data to send
** @param len lenngth of the data to send. The
** Xsendto api is limited to sending at most XIA_MAXBUF bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param addr address (SID) to send the datagram to
** @param addrlen length of the DAG
**
** @returns number of bytes sent on success
** @returns -1 on failure with errno set to an error compatible with those
** returned by the standard sendto call.
**
*/
int Xsendto(int sockfd,const void *buf, size_t len, int flags,
		const struct sockaddr *addr, socklen_t addrlen)
{

	if (validateSocket(sockfd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", sockfd);
		return -1;
	}

	if (connState(sockfd) == CONNECTED) {
		LOGF("socket %d is connected, use Xsend instead!", sockfd);
		errno = EISCONN;
		return -1;
	}

	return _xsendto(sockfd, buf, len, flags, (sockaddr_x *)addr, addrlen);
	
}
