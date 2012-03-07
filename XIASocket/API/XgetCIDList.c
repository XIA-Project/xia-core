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
** @file XgetCIDList.c
** @brief implements XgetCIDList
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
    
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
	size_t cdagListsize = 0;

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
	
	// FIXME: make this use a list instead of inserting the ^ characgter between CIDs

	for (int i = 0; i < numCIDs; i++) {
		cdagListsize += (cDAGv[i].dlen + 1);
	}

	char *cdagList = (char *)calloc(cdagListsize + 1, 1);
    	
	strcpy(cdagList, cDAGv[0].cDAG);
	for (int i = 1; i < numCIDs; i++) {
		strcat(cdagList, "^");
		strcat(cdagList, cDAGv[i].cDAG);
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCID);

	xia::X_Getcid_Msg *x_getcid_msg = xsm.mutable_x_getcid();
  
  	x_getcid_msg->set_numcids(numCIDs);
	x_getcid_msg->set_cdaglist(cdagList);
	x_getcid_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_data(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		free(cdagList);
		return -1;
	}

#if 0	
	// process the reply from click
	if ((rc = click_reply2(sockfd, &type)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}
#endif

	free(cdagList);
	return 0;
}
