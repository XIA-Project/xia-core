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
** @file Xsend.c
** @brief implements Xsend()
*/
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "errno.h"
#include "dagaddr.hpp"

int _xsendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr_x *addr, socklen_t addrlen);

/*!
** @brief Send a message on an Xsocket
**
** The Xsend() call may be used only when the socket is in a connected state
** (so that the intended recipient is known). It only works with an Xsocket of
** type XSOCK_STREAM that has previously been connecteted with Xaccept()
** or Xconnect().
**
** @param sockfd The socket to send the data on
** @param buf the data to send
** @param len length of the data to send. Currently the
** Xsend api is limited to sending at most XIA_MAXBUF bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
**
** @returns number of bytes sent on success
** @returns -1 on failure with errno set to an error compatible with those
** returned by the standard send call.
*/
int Xsend(int sockfd, const void *buf, size_t len, int flags)
{
	#define fmsg  "%s is not currently supported, clearing...\n"
	int rc;

	if (flags & MSG_OOB) {			// Process out-of-band data
		LOGF(fmsg, "MSG_OOB");
	}
	if (flags & MSG_PEEK) {			// Peek at incoming messages
		LOGF(fmsg, "MSG_PEEK");
	}
	if (flags & MSG_DONTROUTE) {	// Don't use local routing
		LOGF(fmsg, "MSG_DONTROUTE");
	}
	if (flags & MSG_DONTROUTE){		// Don't use local routing
		LOGF(fmsg, "MSG_DONTROUTE");
	}
	if (flags & MSG_CTRUNC) {		// Control data lost before delivery
		LOGF(fmsg, "MSG_CTRUNC");
	}
	if (flags & MSG_DONTROUTE) {	// Don't use local routing
		LOGF(fmsg, "MSG_DONTROUTE");
	}
	if (flags & MSG_PROXY) {		// Supply or ask second address
		LOGF(fmsg, "MSG_PROXY");
	}
	if (flags & MSG_TRUNC) {
		LOGF(fmsg, "MSG_TRUNC");
	}
	if (flags & MSG_DONTWAIT) {		// Nonblocking IO
		LOGF(fmsg, "MSG_DONTWAIT");
	}
	if (flags & MSG_EOR) {			// End of record 
		LOGF(fmsg, "MSG_EOR");
	}
	if (flags & MSG_WAITALL) {		// Wait for a full request
		LOGF(fmsg, "MSG_WAITALL");
	}
	if (flags & MSG_FIN) {
		LOGF(fmsg, "MSG_FIN");
	}
	if (flags & MSG_SYN) {
		LOGF(fmsg, "MSG_SYN");
	}
	if (flags & MSG_CONFIRM) {		// Confirm path validity
		LOGF(fmsg, "MSG_CONFIRM");
	}
	if (flags & MSG_RST) {
		LOGF(fmsg, "MSG_RST");
	}
	if (flags & MSG_ERRQUEUE) {		// Fetch message from error queue
		LOGF(fmsg, "MSG_ERRQUEUE");
	}
	if (flags & MSG_NOSIGNAL) {		// Do not generate SIGPIPE
		LOGF(fmsg, "MSG_NOSIGNAL");
	}
	if (flags & MSG_MORE) {			// Sender will send more
		LOGF(fmsg, "MSG_MORE");
	}
	if (flags & MSG_WAITFORONE) {	// Wait for at least one packet to return
		LOGF(fmsg, "MSG_WAITFORONE");
	}
	if (flags & MSG_FASTOPEN) {		// Send data in TCP SYN
		LOGF(fmsg, "MSG_FASTOPEN");
	}
	if (flags & MSG_CMSG_CLOEXEC) {	// Set close_on_exit for file descriptor received through SCM_RIGHTS
		LOGF(fmsg, "MSG_CMSG_CLOEXEC");
	}
	flags = 0;

	if (len == 0)
		return 0;

	len = MIN(len, XIA_MAXBUF);

	if (!buf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (getConnState(sockfd) != CONNECTED) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	int stype = getSocketType(sockfd);
	if (stype == SOCK_DGRAM) {

		// if the DGRAM socket is connected, send to the associated address
		return _xsendto(sockfd, buf, len, flags, dgramPeer(sockfd), sizeof(sockaddr_x));

	} else if (stype != SOCK_STREAM) {
		LOGF("Socket %d must be a stream or datagram socket", sockfd);
		errno = EOPNOTSUPP;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSEND);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Send_Msg *x_send_msg = xsm.mutable_x_send();
	x_send_msg->set_payload(buf, len);

	// send the protobuf containing the user data to click
	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_status(sockfd, seq)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

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

	if (getConnState(sockfd) == CONNECTED) {
		LOGF("socket %d is connected, use Xsend instead!", sockfd);
		errno = EISCONN;
		return -1;
	}

	return _xsendto(sockfd, buf, len, flags, (sockaddr_x *)addr, addrlen);
}


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

	// FIXME: should this return an error, like sendto does?
	// if buf is too big, send only what we can
	if (len > XIA_MAXBUF) {
		LOGF("truncating... requested size (%lu) is larger than XIA_MAXBUF (%d)\n",
				len, XIA_MAXBUF);
		len = XIA_MAXBUF;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSENDTO);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Sendto_Msg *x_sendto_msg = xsm.mutable_x_sendto();

	// FIXME: validate addr
	Graph g(addr);
	std::string s = g.dag_string();

	x_sendto_msg->set_ddag(s.c_str());
	x_sendto_msg->set_payload((const char*)buf, len);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_status(sockfd, seq)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;

	}

	return len;
}


