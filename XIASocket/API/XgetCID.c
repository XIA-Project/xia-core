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
** @brief implements XgetCID and XgetCIDList
*/

#include <errno.h>
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
	struct cDAGvec dv;

	dv.cDAG = cDAG;
	dv.dlen = strlen(cDAG);

	return XgetCIDList(sockfd, &dv, 1);
}

/*!
** @brief Load a list of CIDs to the local machine.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAGv - list of CIDs to retrieve
** @param numCIDs - number of CIDs in cDAGv
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int XgetCIDList(int sockfd, const struct cDAGvec *cDAGv, int numCIDs)
{
	int rc;
	const char *buf="CID request";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket", sockfd);
		return -1;
	}

	if (numCIDs == 0)
		return 0;

	if (!cDAGv) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}
	
	// If the CID list is too long for a UDP packet to click, replace with multiple calls
	if (numCIDs > 300) //TODO: Make this more precise
	{
		rc = 0;
		int i;
		for (i = 0; i < numCIDs; i += 300)
		{
			int num = (numCIDs-i > 300) ? 300 : numCIDs-i;
			int rv = XgetCIDList(sockfd, &cDAGv[i], num);

			if (rv == -1) {
				perror("Xgetcid(): getcid failed");
				return(-1);
			} else {
				rc += rv;
			}
		}

		return rc;
	}
	
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCID);

	xia::X_Getcid_Msg *x_getcid_msg = xsm.mutable_x_getcid();
  
	for (int i = 0; i < numCIDs; i++) {
		if (cDAGv[i].cDAG != NULL)
			x_getcid_msg->add_dag(cDAGv[i].cDAG);
		else {
			LOGF("NULL pointer at cDAGv[%d]\n", i);
		}
	}

	if (x_getcid_msg->dag_size() == 0) {
		// FIXME: what error should this relate to?
		errno = EFAULT;
		LOG("No dags specified\n");
		return -1;
	}

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
