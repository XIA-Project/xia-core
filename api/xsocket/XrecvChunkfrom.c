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
** XrecvChunkfrom() retrieves data from an Xsocket of type XSOCK_CHUNK. 
**
** XrecvChunkFrom() does not currently have a non-blocking mode, and will block
** until a data is available on sockfd. 
**
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


// FIXME: It's better if the whole source dag is returned. Have to modify push message and xiacontentmodule
int XrecvChunkfrom(int sockfd, void *rbuf, size_t len, int flags, ChunkInfo *ci)
{
	int rc;
	int numbytes;


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
	xsm.set_type(xia::XRECVCHUNKFROM);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Recvchunkfrom_Msg *msg = xsm.mutable_x_recvchunkfrom();

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	LOG("recvchunk API waiting for chunk ");
	xsm.Clear();
	if ((numbytes = click_reply(sockfd, seq, &xsm)) < 0) {
		if (isBlocking(sockfd) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
			LOGF("Error retrieving recv data from Click: %s", strerror(errno));
		}
		return -1;
	}

//	xia::X_Result_Msg *r = xsm.mutable_x_result();
	msg = xsm.mutable_x_recvchunkfrom();

	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();
	//FIXME: This fixes CID:cid to cid. The API should change to return the SDAG instead
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