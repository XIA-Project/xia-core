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
** @file XrequestChunk.c
** @brief implements XrequestChunk() and XrequestChunks()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief request that a content chunk be loaded into the local machine's
** content cache.
**
** XrequestChunk() is called by a client application to load a content chunk 
** into the XIA content cache. It does not return the requested data, it only
** causes the chunk to be loaded into the local content cache. XgetChunkStatus() 
** may be called to get the status of the chunk to determine when it becomes 
** available. Once the chunk is ready to be read, XreadChunk() should be called 
** get the actual content chunk data.
**
** XrequestChunk() is a simple wrapper around the XrequestChunks() API call.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param dag Content ID of this chunk
** @param dagLen length of dag (currently not used)
**
** @returns 0 on success
** @returns -1 if the requested chunk could not be located or a socket error
** occurred. If the error is a socket error, errno set will be set with an 
** appropriate code.
*/
int XrequestChunk(int sockfd, char* dag, size_t /* dagLen */)
{
	ChunkStatus cs;

	cs.cid = dag;
	cs.cidLen = strlen(dag);

	return XrequestChunks(sockfd, &cs, 1);
}

/*!
** @brief request that a list of content chunks be loaded into the local 
** machine's content cache.
**
** XrequestChunks() is called by a client application to load a content chunk 
** into the XIA content cache. It does not return the requested data, it only
** causes the chunk to be loaded into the local content cache. XgetChunkStatuses() 
** may be called to get the status of the chunk to determine when it becomes 
** available. Once the chunk is ready to be read, XreadChunk() should be called 
** get the actual content chunk data.
**
** XrequestChunk() can be used when only a single chunk is requested.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param chunks A list of content DAGs to retrieve
** @param numChunks number of CIDs in the chunk list
**
** @returns 0 on success
** @returns -1 if one or more of the requested chunks could not be located 
** or a socket error occurred. If the error is a socket error, errno set 
** will be set with an appropriate code.
*/
int XrequestChunks(int sockfd, const ChunkStatus *chunks, int numChunks)
{
	int rc;
	const char *buf="Chunk request";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket", sockfd);
		return -1;
	}

	if (numChunks == 0)
		return 0;

	if (!chunks) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}
	
	// If the DAG list is too long for a UDP packet to click, replace with multiple calls
	if (numChunks > 300) //TODO: Make this more precise
	{
		rc = 0;
		int i;
		for (i = 0; i < numChunks; i += 300)
		{
			int num = (numChunks-i > 300) ? 300 : numChunks-i;
			int rv = XrequestChunks(sockfd, &chunks[i], num);

			if (rv == -1) {
				perror("XrequestChunk(): requestChunk failed");
				return(-1);
			} else {
				rc += rv;
			}
		}

		return rc;
	}
	
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREQUESTCHUNK);

	xia::X_Requestchunk_Msg *x_requestchunk_msg = xsm.mutable_x_requestchunk();
  
	for (int i = 0; i < numChunks; i++) {
		if (chunks[i].cid != NULL)
			x_requestchunk_msg->add_dag(chunks[i].cid);
		else {
			LOGF("NULL pointer at chunks[%d]\n", i);
		}
	}

	if (x_requestchunk_msg->dag_size() == 0) {
		// FIXME: what error should this relate to?
		errno = EFAULT;
		LOG("No dags specified\n");
		return -1;
	}

	x_requestchunk_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

#if 0	
	// process the reply from click
	if ((rc = click_reply2(sockfd, &type)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}
#endif

	return 0;
}
