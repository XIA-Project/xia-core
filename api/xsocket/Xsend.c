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
	int rc;

	if (flags) {
		errno = EOPNOTSUPP;
		return -1;
	}

	if (len == 0)
		return 0;

	len = MIN(len, XIA_MAXBUF);

	if (!buf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (connState(sockfd) != CONNECTED) {
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

	xia::X_Send_Msg *x_send_msg = xsm.mutable_x_send();
	x_send_msg->set_payload(buf, len);

	// send the protobuf containing the user data to click
	if ((rc = click_send(sockfd, &xsm)) < 0) {
		if (!WOULDBLOCK()) {
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}
#if 0
	// process the reply from click
	if ((rc = click_reply2(sockfd, &type)) < 0) {
		LOGF("Error retreiving data from Click: %s", strerror(errno));
		return -1;
	}

	if (type != xia::XSEND) {
		LOGF("Expected type %d, got %d", xia::XSEND, type);
		// what do we do in this case?
		// we might have sent the data, but can't be sure
	}
#endif
	return len;
}
