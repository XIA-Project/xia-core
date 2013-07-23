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
** @file XgetChunkStatus.c
** @brief implements XgetChunkStatus() and XgetChunkStatuses()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"


/*!
** @brief Checks the status of the specified CID.
**
** XgetChunkStatus returns an integer indicating if the specified content
** chunk is available to be read. It is a simple wrapper around the
** XgetChunkStatuses() function which does the actual work.
**
** @note This function Should be called after calling XrequestChunk() or
** XrequestChunks(). Otherwise the content chunk will never be loaded into
** the content cache and will result in a REQUEST_FAILED error.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param dag Content ID of the chunk to check. This should be a full dag
** for the desired chunk, not just the CID.
** @param dagLen length of dag (currently not used)
**
** @returns READY_TO_READ if the requested chunk is ready to be read.
** @returns INVALID_HASH if the CID hash does not match the content payload.
** @returns WAITING_FOR_CHUNK if the requested chunk is still in transit.
** @returns REQUEST_FAILED if the specified chunk has not been requested
** @returns -1 if a socket error occurs. In that case errno is set with the appropriate code.
*/
int XgetChunkStatus(int sockfd, char* dag, size_t /* dagLen */)
{
	ChunkStatus cs;

	cs.cid = dag;
	cs.cidLen = strlen(dag);

	return XgetChunkStatuses(sockfd, &cs, 1);
}

/*
** @brief Checks the status for each of the requested CIDs.
**
** XgetChunkStatuses updates the cDAGv list with the status for each
** of the requested CIDs. An overall status value is returned, and
** the cDAGv list can be examined to check the status of each individual CID.
**
** @note This function Should be called after calling XrequestChunk() or
** XrequestChunks(). Otherwise the content chunk will never be loaded into
** the content cache and will result in a REQUEST_FAILED error.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAGv - list of CIDs to check. On return, also  contains the status for
** each of the specified CIDs.
** @param numCIDs - number of CIDs in cDAGv
**
** @returns a bitfield indicating the status of the chunks.
** @returns if return equals READY_TO_READ, all chunks are avilable to read
** @returns otherwise the return value contains a bitfield of status codes
** @returns if REQUEST_FAILED is set, one or more of the requested chunks could not be found
** @returns if WAITING_FOR_CHUNK is set, one or more of the requested chunks is still in transit
** @returns if INVALID_HASH is set, the content of one or more chunks does not match the hash in the CID
** @returns REQUEST_FAILED if one of the specified chunks has not been requested,
** @returns -1  if a socket error occurs. In that case errno is set with the appropriate code.
*/
int XgetChunkStatuses(int sockfd, ChunkStatus *statusList, int numCIDs)
{
	int rc;
	char buffer[MAXBUFLEN];

	const char *buf="CID list request status query";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}

	if (numCIDs == 0)
		return 0;

	if (!statusList) {
		LOG("statusList is null!");
		errno = EFAULT;
		return -1;
	}

	// protobuf message
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCHUNKSTATUS);

	xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg = xsm.mutable_x_getchunkstatus();

	for (int i = 0; i < numCIDs; i++) {
		if (statusList[i].cid) {
			x_getchunkstatus_msg->add_dag(statusList[i].cid);
		} else {
			LOGF("cDAGv[%d] is NULL", i);
		}
	}

	if (x_getchunkstatus_msg->dag_size() == 0) {
		LOG("no dags were specified!");
		errno = EFAULT;
		return -1;
	}

	x_getchunkstatus_msg->set_payload((const char*)buf, strlen(buf) + 1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, xia::XGETCHUNKSTATUS, buffer, sizeof(buffer))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(buffer);

	if (xia_socket_msg1.type() == xia::XGETCHUNKSTATUS) {

		xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg1 = xia_socket_msg1.mutable_x_getchunkstatus();

		char status_tmp[100];
		int status_for_all = READY_TO_READ;

		for (int i = 0; i < numCIDs; i++) {
			strcpy(status_tmp, x_getchunkstatus_msg1->status(i).c_str());

			if (strcmp(status_tmp, "WAITING") == 0) {
				statusList[i].status = WAITING_FOR_CHUNK;

				status_for_all &= ~READY_TO_READ;
				status_for_all |= WAITING_FOR_CHUNK;

			} else if (strcmp(status_tmp, "INVALID_HASH") == 0) {
				statusList[i].status = INVALID_HASH;

				status_for_all &= ~READY_TO_READ;
				status_for_all |= INVALID_HASH;

			} else if (strcmp(status_tmp, "READY") == 0) {
				statusList[i].status = READY_TO_READ;

			} else if (strcmp(status_tmp, "FAILED") == 0) {
				statusList[i].status = REQUEST_FAILED;

				status_for_all &= ~READY_TO_READ;
				status_for_all |= REQUEST_FAILED;

			} else {
				statusList[i].status = REQUEST_FAILED;
				status_for_all &= ~READY_TO_READ;
				status_for_all |= REQUEST_FAILED;
			}
		}

		rc = status_for_all;
	} else
		rc = -1;

	return rc;
}
