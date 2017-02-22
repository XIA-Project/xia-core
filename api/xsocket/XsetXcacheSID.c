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
#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"


/*!
** @brief retrieve the AD and HID associated with this socket.
**
** The HID and AD are assigned by the XIA stack. This call retrieves them
** so that they can be used for creating DAGs or for other purposes in user
** applications.
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
** @param localhostAD buffer to receive the AD for this host
** @param lenAD size of the localhostAD buffer
** @param localhostHID buffer to receive the HID for this host
** @param lenHID size of the localhostHID buffer
**
** @returns 0 on success
** @returns -1 on failure with errno set
**
*/
int XsetXcacheSID(int sockfd, char *xcacheSID, unsigned lenXcacheSID) {
	int seq, rc;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	if (xcacheSID == NULL) {
		LOG("NULL pointer!");
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xia::X_SetXcacheSid_Msg *msg = xsm.mutable_x_setxcachesid();
	xsm.set_type(xia::XSETXCACHESID);
	msg->set_sid(xcacheSID, lenXcacheSID);
	seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	return rc;
}
