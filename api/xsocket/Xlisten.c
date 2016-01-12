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
** @brief implements Xlisten()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Accept a conection from a remote Xsocket
**
** The Xaccept system call is is only valid with Xsockets created with
** the XSOCK_STREAM transport type. It accepts the first available connection
** request for the listening socket, sockfd, creates a new connected socket,
** and returns a new Xsocket descriptor referring to that socket. The newly
** created socket is not in the listening state. The original socket
** sockfd is unaffected by this call.
**
** Xaccept does not currently have a non-blocking mode, and will block
** until a connection is made. However, the standard socket API calls select
** and poll may be used with the Xsocket. Either function will deliver a
** readable event when a new connection is attempted and you may then call
** Xaccept() to get a socket for that connection.
**
** @note Unlike standard sockets, there is currently no Xlisten function.
** Callers must create the listening socket by calling Xsocket with the
** XSOCK_STREAM transport_type and bind it to a source DAG with Xbind(). XAccept
** may then be called to wait for connections.
**
** @param sockfd	an Xsocket() previously created with the XSOCK_STREAM type,
** and bound to a local DAG with Xbind()
** @param addr if non-NULL, points to a block of memory that will contain the
** address of the peer on return
** @param addrlen on entry, contains the size of addr, on exit contains the actual
** size of the address. addr will be truncated, if the size passed in is smaller than
** the actual size.
**
** @returns a non-negative integer that is the new Xsocket id
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard accept call.
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