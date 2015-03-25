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
** @file Xaccept.c
** @brief implements Xaccept()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

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
int Xaccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	// Xaccept accepts the connection, creates new socket, and returns it.

	struct sockaddr_in my_addr;
	socklen_t len;
	int new_sockfd;

printf("XACCEPT %d\n", sockfd);
	// if an addr buf is passed, we must also have a valid length pointer
	if (addr != NULL && addrlen == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOG("Xaccept is only valid with stream sockets.");
		return -1;
	}

	// Tell click we're ready to accept a pending connection
	xia::XSocketMsg ready_xsm;
	ready_xsm.set_type(xia::XREADYTOACCEPT);
	unsigned seq = seqNo(sockfd);
	ready_xsm.set_sequence(seq);

	if (click_send(sockfd, &ready_xsm) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// we'll block waiting for click to tell us there's a pending connection
	if (click_status(sockfd, seq) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	// Create new socket (this is a socket between API and Xtransport)
	if ((new_sockfd = (_f_socket)(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("Error creating new socket: %s", strerror(errno));
		return -1;
	}

printf("accept fd=%d SOCK_STREAM=%d\n", new_sockfd, SOCK_STREAM);
	allocSocketState(new_sockfd, SOCK_STREAM);

	
	// bind to an unused random port number
	my_addr.sin_family = PF_INET;
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	my_addr.sin_port = 0;

	if (bind(new_sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
		(_f_close)(new_sockfd);

		LOGF("Error binding new socket to local port: %s", strerror(errno));
		return -1;
	}



	// Tell click what the new socket's port is (but we'll tell click
	// over the old socket)
	len = sizeof(my_addr);
	if((_f_getsockname)(new_sockfd, (struct sockaddr *)&my_addr, &len) < 0) {
		(_f_close)(new_sockfd);
		LOGF("Error retrieving new socket's UDP port: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xia_socket_msg;
	xia_socket_msg.set_type(xia::XACCEPT);
	seq = seqNo(new_sockfd);
	xia_socket_msg.set_sequence(seq);
	
	xia::X_Accept_Msg *x_accept_msg = xia_socket_msg.mutable_x_accept();
	x_accept_msg->set_new_port(((struct sockaddr_in)my_addr).sin_port);

	if (click_send(sockfd, &xia_socket_msg) < 0) {
//		(_f_close)(new_sockfd);
		close(new_sockfd);
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

// FIXME: change to use click reply so we get the peer dag!
	if (click_status(sockfd, seq) < 0) {
		(_f_close)(new_sockfd);
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	// FIXME: add code to get the peername, and fill in the sockaddr
	if (addr != NULL) {
		if (*addrlen < sizeof(sockaddr_x)) {
			LOG("addr is not large enough to hold a sockaddr_x");
		}
	}
	if (addrlen)
		*addrlen = 0;


	setConnState(new_sockfd, CONNECTED);

	return new_sockfd;
}

/*!
** @brief Accept a conection from a remote Xsocket
**
** The Xaccept4 system call is is only valid with Xsockets created with
** the XSOCK_STREAM transport type. It accepts the first available connection
** request for the listening socket, sockfd, creates a new connected socket,
** and returns a new Xsocket descriptor referring to that socket. The newly
** created socket is not in the listening state. The original socket
** sockfd is unaffected by this call.
**
** Xaccept4 does not currently have a non-blocking mode, and will block
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
** @param flags retained for compatability, Xaccept4 will return an error if non-zero
**
** @returns a non-negative integer that is the new Xsocket id
** @returns -1 on error with errno set to an error compatible with those
** returned by the standard accept call.
*/
int Xaccept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	if (flags != 0) {
		LOG("error: flags are not currently allowed in the XIA version of accept4");
		errno = EINVAL;
		return - 1;
	}

	return Xaccept(sockfd, addr, addrlen);
}
