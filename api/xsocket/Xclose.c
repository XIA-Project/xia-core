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
** @file Xclose.c
** @brief implements Xclose()
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "Xkeys.h"
#include <errno.h>

/*!
** @brief Close an Xsocket.
**
** Causes the XIA transport to tear down the underlying XIA socket state and
** also closes the UDP control socket used to talk to the transport.
**
** @param sockfd	The control socket
**
** @returns 0 on success
** @returns -1 on error with errno set to a value compatible with the standard
** close API call.
*/
int Xclose(int sockfd)
{
	int rc = -1;
	int sock = 0;

	if (getSocketType(sockfd) == XSOCK_INVALID)
	{
		LOGF("The socket %d is not a valid Xsocket", sockfd);
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xia::X_Close_Msg *xcm;
	xsm.set_type(xia::XCLOSE);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xcm = xsm.mutable_x_close();

	unsigned short port;
	port = getPort(sockfd);
	xcm->set_port(port);

	sock = MakeApiSocket(SOCK_DGRAM);

	if ((rc = click_send(sock, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		goto done;

	} else if ((rc = click_reply(sock, seq, &xsm)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		goto done;
	}

	if ((isTempSID(sockfd) || xcm->delkeys()) && xcm->refcount() <= 0) {

		LOGF("Deleting keys for %s", getTempSID(sockfd));

		// Delete any temporary keys created for this sockfd
		if (XremoveSID(getTempSID(sockfd))) {
			LOGF("ERROR removing key files for %s", getTempSID(sockfd));
		}
	}
done:
	(_f_close)(sockfd);
	freeSocketState(sockfd);


	if (sock > 0) {
		freeSocketState(sock);
		(_f_close)(sock);
	}

	return rc;
}
