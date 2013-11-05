/*
** Copyright 2013 Carnegie Mellon University
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
** @file XrecvChunkfrom.c
** @brief implements XrecvChunkfrom()
*/

/*
 * recvfrom like datagram receiving function for XIA
 * does not fill in DAG fields yet
 */

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

/*!
** @brief receives datagram data on an Xsocket.
**
** XrecvChunkfrom() retrieves data from an Xsocket of type XSOCK_DGRAM. Unlike the 
** standard recvfrom API, it will not work with sockets of type XSOCK_STREAM.
**
** XrecvChunkFrom() does not currently have a non-blocking mode, and will block
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
** @param ChunkInfo loads the received the chunkinfo
**
** @returns number of bytes received
** @returns -1 on failure with errno set.
*/


// FIXME: Seems like recvchunk gets CID:cid instead of just cid in the ChunkInfo which shouldn't happen
int XrecvChunkfrom(int sockfd, void *rbuf, size_t len, int flags, ChunkInfo *ci)
{
	int rc;
	char UDPbuf[MAXBUFLEN];


	if (flags != 0) {
		LOG("flags is not suppored at this time");
		errno = EINVAL;
		return -1;
	}

	if (!rbuf || !ci ) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}	
	
	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}

	if (len == 0)
		return 0;

	if (!rbuf || !ci) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPUSHCHUNKTO);


	LOG("recvchunk API waiting for chunk ");
	if ((rc = click_reply(sockfd, UDPbuf, sizeof(UDPbuf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	std::string str(UDPbuf, rc);

	xsm.Clear();
	xsm.ParseFromString(str);
	
	xia::X_Pushchunkto_Msg *msg = xsm.mutable_x_pushchunkto();
	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();
	//FIXME: This fixes CID:cid but not sure if this should be done here. 
 	strcpy(ci->cid, msg->cid().substr(4,msg->cid().npos).c_str());
// 	strcpy(ci->cid, msg->cid().c_str());
	ci->size = msg->length();
	ci->timestamp.tv_sec=msg->timestamp();
	ci->timestamp.tv_usec = 0;
	ci->ttl = msg->ttl();

	if (paylen > len) {
		LOGF("CID is %u bytes, but rbuf is only %lu bytes", paylen, len);
		errno = EFAULT;
		return -1;
	}

	memcpy(rbuf, payload, paylen);
	return paylen;
}