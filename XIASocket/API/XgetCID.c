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
** @file XgetCID.c
** @brief implements XgetCID
*/

#include<errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Bring a content chunk local to this machine.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAG - Content ID of this chunk
** @param dlen - length of sDAG (currently not used)
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int XgetCID(int sockfd, char* cDAG, size_t /* dlen */)
{
	int rc;
	const char *buf="CID request";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket", sockfd);
		return -1;
	}

	if (!cDAG) {
		LOG("cDAG is null!");
		errno = EFAULT;
		return -1;
	}

	// protobuf message
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCID);

	xia::X_Getcid_Msg *x_getcid_msg = xsm.mutable_x_getcid();
  
  	x_getcid_msg->set_numcids(1);
	x_getcid_msg->set_cdaglist(cDAG);
	x_getcid_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_data(sockfd, &xsm)) < 0) {
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




