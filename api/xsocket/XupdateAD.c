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
 @file XupdateAD.c
 @brief Implements XreadLocalHostAddr()
*/
#include <errno.h>
#include <fcntl.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

int XupdateAD(int sockfd, char *newad, char *new4id)
{
  int rc;

  if (!newad) {
    LOG("new ad is NULL!");
    errno = EFAULT;
    return -1;
  }

  if (getSocketType(sockfd) == XSOCK_INVALID) {
    LOG("The socket is not a valid Xsocket");
    errno = EBADF;
    return -1;
  }

  xia::XSocketMsg xsm;
  xsm.set_type(xia::XCHANGEAD);

  xia::X_Changead_Msg *x_changead_msg = xsm.mutable_x_changead();
  x_changead_msg->set_ad(newad);
  x_changead_msg->set_ip4id(new4id);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		if (!WOULDBLOCK()) {
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}

  return 0;
}


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
int XreadLocalHostAddr(int sockfd, char *localhostAD, unsigned lenAD, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID)
{
	int rc;
	char UDPbuf[MAXBUFLEN];

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREADLOCALHOSTADDR);

	int flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, xia::XREADLOCALHOSTADDR, UDPbuf, sizeof(UDPbuf))) < 0) {
		fcntl(sockfd, F_SETFL, flags);
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	fcntl(sockfd, F_SETFL, flags);

	xia::XSocketMsg xsm1;
	xsm1.ParseFromString(UDPbuf);
	if (xsm1.type() == xia::XREADLOCALHOSTADDR) {
		xia::X_ReadLocalHostAddr_Msg *_msg = xsm1.mutable_x_readlocalhostaddr();
		strncpy(localhostAD, (_msg->ad()).c_str(), lenAD);
		strncpy(localhostHID, (_msg->hid()).c_str(), lenHID);
		strncpy(local4ID, (_msg->ip4id()).c_str(), len4ID);
		rc = 0;
	} else {
		rc = -1;
	}
	return rc;

}


/*!
** @tell if this node is an XIA-IPv4 dual-stack router
**
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
**
** @returns 1 if this is an XIA-IPv4 dual-stack router
** @returns 0 if this is an XIA router
** @returns -1 on failure with errno set
**
*/
int XisDualStackRouter(int sockfd) {
	int rc;
	char UDPbuf[MAXBUFLEN];

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XISDUALSTACKROUTER);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, xia::XISDUALSTACKROUTER, UDPbuf, sizeof(UDPbuf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xsm1;
	xsm1.ParseFromString(UDPbuf);
	if (xsm1.type() == xia::XISDUALSTACKROUTER) {
		xia::X_IsDualStackRouter_Msg *_msg = xsm1.mutable_x_isdualstackrouter();
		rc = _msg->flag();
	} else {
		rc = -1;
	}
	return rc;

}
