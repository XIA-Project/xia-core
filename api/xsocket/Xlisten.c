/* ts=4 */
/*
** Copyright 2015 Carnegie Mellon University
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
** @file Xlisten.c
** @brief Xlisten() - listen for connections on a socket
*/

#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include "Xinit.h"
#include "Xutil.h"
/*! \endcond */

/*!
** @brief listen for connections on a socket
**
**  Xlisten() marks the socket referred to by sockfd as a passive socket,
** that is, as a socket that will be used to accept incoming connection
**requests using Xaccept().
**
** The backlog argument defines the maximum length to which the queue of
** pending connections for sockfd may grow. If a connection request arrives
** when the queue is full, the client will receive an error with an
** indication of ECONNREFUSED.
**
** @note See the man page for the standard listen() call for more details.
**
** @param sockfd an Xsocket previously created with the SOCK_STREAM type,
** and bound to a local DAG with Xbind()
** @param backlog the number of outstanding connections allowed in the listen queue
**
** @returns 0 on success
** @returns -1 on error with errno set appropriately
*/
int Xlisten(int sockfd, int backlog)
{
	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xaccept is only valid with stream sockets.");
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XLISTEN);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Listen_Msg *x_listen_msg = xsm.mutable_x_listen();
	x_listen_msg->set_backlog(backlog);

	if (click_send(sockfd, &xsm) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// we'll block waiting for click to tell us there's a pending connection
	if (click_status(sockfd, seq) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	return 0;
}
