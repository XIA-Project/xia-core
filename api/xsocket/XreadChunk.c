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
** @file XreadChunk.c
** @brief implements XreadChunk()
*/

#include<errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Reads the contents of the specified CID into rbuf. Must be called
** after XrequestChunk() or XrequestChunks().
**
** the CID specified in cid must be a full DAG, not a fragment such as is
** returned by XputChunk. For instance: "RE ( AD:AD0 HID:HID0 ) CID:<hash>"
** where <hash> is the 40 character hash of the content chunk generated
** by the sender. The XputChunk() API call only returns <hash>. Either the
** client or server application must generate the full DAG that is passed
** to this API call.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param rbuf buffer to receive the data
** @param len length of rbuf
** @param flags currently unused
** @param cid the CID to retrieve. cid should be a full DAG, not a fragment.
** @param cidLen length of cid (currently unused)
**
** @returns number of bytes in the CID
** @returns -1 on error with errno set
**
*/
int XreadChunk(int sockfd, void *rbuf, size_t len, int /* flags */,
		char * cid, size_t /* cidLen */)
{
	int rc;
	char UDPbuf[MAXBUFLEN];

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}

	if (len == 0)
		return 0;

	if (!rbuf || !cid) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREADCHUNK);

	xia::X_Readchunk_Msg *x_readchunk_msg = xsm.mutable_x_readchunk();

	x_readchunk_msg->set_dag(cid);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		if (!WOULDBLOCK()) {
			// FIXME: correct behavior here, if we get past here, but
			LOGF("Error talking to Click: %s", strerror(errno));
		}
		return -1;
	}

	if ((rc = click_reply(sockfd, xia::XREADCHUNK, UDPbuf, sizeof(UDPbuf))) < 0) {
		if (!WOULDBLOCK()) {
			LOGF("Error retrieving status from Click: %s", strerror(errno));
		}
		return -1;
	}

	std::string str(UDPbuf, rc);

	xsm.Clear();
	xsm.ParseFromString(str);

	xia::X_Readchunk_Msg *msg = xsm.mutable_x_readchunk();
	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();

	if (paylen > len) {
		LOGF("CID is %d bytes, but rbuf is only %d bytes", paylen, len);
		errno = EFAULT;
		return -1;
	}

	memcpy(rbuf, payload, paylen);
	return paylen;
}

