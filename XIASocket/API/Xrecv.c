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
** @brief implements Xrecv
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief receives data on an Xsocket. Xconnect must be called before
** using this function.
**
** @param sockfd - The socket to receive with
** @param rbuf - where to put the received data
** @param len - maximum amount of data to receive. the amount of data
** returned may be less than len bytes.
** @param flags - (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
**
** NOTE: in cases where more data is received than specified by the caller,
** the excess data will be stored in the socket state structure and will 
** be returned from there rather than from Click. Once the socket state
** is drained, requests will be sent through to Click again.
**
** @returns number of bytes received
** @returns -1 on failure with errno set.
*/
int Xrecv(int sockfd, void *rbuf, size_t len, int  /*flags */)
{
	struct addrinfo hints;
	int numbytes;
	socklen_t addr_len;
	char UDPbuf[MAXBUFLEN];
	
	struct sockaddr_in their_addr;

	if (len == 0)
		return 0;

	if (!rbuf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	if (validateSocket(sockfd, XSOCK_STREAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a stream socket", sockfd);
		return -1;
	}

	if (!isConnected(sockfd)) {
		LOGF("Socket %d is not connected", sockfd);
		errno = ENOTCONN;
		return -1;
	}

	// see if we have bytes leftover from a previous Xrecv call
	if ((numbytes = getSocketData(sockfd, (char *)rbuf, len)) > 0)
		return numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , 0,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		LOGF("Error retreiving data from Click: %s", strerror(errno));
		return -1;
	}
	
	// FIXME: make this use protobufs
	size_t paylen = 0, i = 0;
	char *tmpbuf = (char*)UDPbuf;

	while (tmpbuf[i] != '^')
		i++;
	paylen = numbytes - i - 1;
	int offset= i + 1;

	if (paylen <= len)
		memcpy(rbuf, UDPbuf + offset, paylen);
	else {
		// we got back more data than the caller requested
		// stash the extra away for subsequent Xrecv calls
		memcpy(rbuf, UDPbuf + offset, len);
		paylen -= len;
		offset += len;
		setSocketData(sockfd, UDPbuf + offset, paylen);
		paylen = len;
	}

	return paylen;
}
