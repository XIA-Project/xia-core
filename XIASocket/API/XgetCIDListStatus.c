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
** @file XgetCIDListStatus.c
** @brief implements XgetCIDListStatus
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Checks the status of the specified CID. Should be called after
** calling XgetCID or XgetCIDList.
** It checks whether each of the requested CIDs is waiting to be read or still
** on the way.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAGv - CIDs to check. On return contains the status for each of the
** specified CIDs.
** @param numCIDs - number of CIDs in cDAGv
**
** @returns 1 if all CIDs in cDAGv are ready to be read
** @returns 0 if all CIDs in cDAGv are valid but one or more are in 
** waiting state
** @returns -1 on socket error with or if an invalid CID was specified
*/
int XgetCIDListStatus(int sockfd, struct cDAGvec *cDAGv, int numCIDs)
{
	int rc;
	char buffer[MAXBUFLEN];
	char *statusbuf, *cdagList;
	
	const char *buf="CID list request status query";//Maybe send more useful information here.
	size_t cdagListsize = 0;

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}
	
	if (numCIDs == 0)
		return 0;

	if (!cDAGv) {
		LOG("cDAGv is null!");
		errno = EFAULT;
		return -1;
	}

	for (int i = 0; i < numCIDs; i++) {
		cdagListsize += (cDAGv[i].dlen + 1);
	}

	// FIXME: make this code use lists in the protobuf

	if ((cdagList = (char *)calloc(cdagListsize + 1, 1)) == NULL)
		return -1;
	if ((statusbuf = (char *)calloc(numCIDs, 12)) == NULL) {
		free(cdagList);
		return -1;
	}
    	
   	strcpy(statusbuf, "WAITING");
   	strcpy(cdagList, cDAGv[0].cDAG);

   	for (int i = 1; i < numCIDs; i++) {
   		strcat(cdagList, "^");
		strcat(cdagList, cDAGv[i].cDAG);
		
		strcat(statusbuf, "^");
		strcat(statusbuf, "WAITING");
	}

	// protobuf message
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCIDSTATUS);

	xia::X_Getcidstatus_Msg *x_getcidstatus_msg = xsm.mutable_x_getcidstatus();
  
  	x_getcidstatus_msg->set_numcids(numCIDs);
	x_getcidstatus_msg->set_cdaglist(cdagList);
	x_getcidstatus_msg->set_status_list(statusbuf);
	x_getcidstatus_msg->set_payload((const char*)buf, strlen(buf) + 1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_data(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		free(statusbuf);
		free(cdagList);
		return -1;
	}
     	 
	if ((rc = click_reply(sockfd, buffer, sizeof(buffer))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		free(statusbuf);
		free(cdagList);
		return -1;
	}

	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(buffer);

	if (xia_socket_msg1.type() == xia::XGETCIDSTATUS) {
		
		xia::X_Getcidstatus_Msg *x_getcidstatus_msg1 = xia_socket_msg1.mutable_x_getcidstatus();
		strcpy(statusbuf, x_getcidstatus_msg1->status_list().c_str()); 
		    
		char status_tmp[100];
		char *start = statusbuf;
		char *pch;
		int status_for_all = READY_TO_READ;
		
		for (int i = 0; i < numCIDs; i++) {
			pch = strchr(start,'^');
			if (pch != NULL) {
				// there's more CID status followed
				strncpy (status_tmp, start, pch - statusbuf);
				status_tmp[pch - start]='\0';
				start = pch + 1;
		    	
			} else {
				// this is the last CID status in this batch.
				strcpy (status_tmp, start);
			}
		    	
			if (strcmp(status_tmp, "WAITING") == 0) {
				cDAGv[i].status = WAITING_FOR_CHUNK;
				
				if (status_for_all != REQUEST_FAILED) { 
					status_for_all = WAITING_FOR_CHUNK;
				}
		    		
			} else if (strcmp(status_tmp, "READY") == 0) {
				cDAGv[i].status = READY_TO_READ;
		    	
			} else if (strcmp(status_tmp, "FAILED") == 0) {
				cDAGv[i].status = REQUEST_FAILED;
				status_for_all = REQUEST_FAILED;
			
			} else {
				cDAGv[i].status = REQUEST_FAILED;
				status_for_all = REQUEST_FAILED;
			}
		}
		    
		rc = status_for_all;
	} else
		rc = -1;
	
	free(statusbuf);
	free(cdagList);
	return rc; 
}
