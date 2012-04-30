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
** @file Xrecvfrom.c
** @brief implements Xrecvfrom()
*/

/*
 * recvfrom like datagram receiving function for XIA
 * does not fill in DAG fields yet
 */

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief receives datagram data on an Xsocket.
**
** Xrecvfrom() retrieves data from an Xsocket of type XSOCK_DGRAM. Unlike the 
** standard recvfrom API, it will not work with sockets of type XSOCK_STREAM.
**
** XrecvFrom() does not currently have a non-blocking mode, and will block
** until a data is available on sockfd. However, the standard socket API
** calls select and poll may be used with the Xsocket. Either function
** will deliver a readable event when a new connection is attempted and
** you may then call XrecvFrom() to get the data.
**
** NOTE: in cases where more data is received than specified by the caller,
** the excess data will be stored in the socket state structure and will 
** be returned from there rather than from Click. Once the socket state
** is drained, requests will be sent through to Click again.
**
** @param sockfd The socket to receive with
** @param rbuf where to put the received data
** @param len maximum amount of data to receive. The amount of data
** returned may be less than len bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param sDAG on success, contains the DAG of the sender
** @param slen contians the size of sDAG when called, replaced with the length
** of the received sDAG on return. slen MUST be set to the size of sDAG before
** calling XrecvFrom(). If slen is smaller than the length of the source DAG,
** the returned DAG is truncated and slen will contain length of the actual DAG.
**
** @returns number of bytes received
** @returns -1 on failure with errno set.
*/
int Xrecvfrom(int sockfd, void *rbuf, size_t len, int flags,
	char* sDAG, size_t* slen)
{
    struct addrinfo hints;
    int numbytes;
    socklen_t addr_len;
    char UDPbuf[MAXBUFLEN];
    struct sockaddr_in their_addr;

	if (!rbuf || !sDAG || !slen) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}	

	if (validateSocket(sockfd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", sockfd);
		return -1;
	}
	
	// see if we have bytes leftover from a previous Xrecv call
	if ((numbytes = getSocketData(sockfd, (char *)rbuf, len)) > 0) {
		// FIXME: we need to also have stashed away the sDAG and
		// return it as well
		*sDAG = 0;
		*slen = 0;
		return numbytes;
	}
	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
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

	if (i + 1 > *slen) {
		LOGF("sDAG buffer is not large enough, sDAG has been truncated: has %d, needs %d\n", *slen, i + 1);
//		errno = EFAULT;
//		return -1;
	}
    	
	// leave room for the null terminator, truncate the DAG if necessary
	int sz = MIN(i, *slen - 1);
	strncpy(sDAG, UDPbuf, sz);
    sDAG[sz] = 0;
    *slen = i;

    return paylen;
}
