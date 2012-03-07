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
** @file XgetCIDStatus.c
** @brief implements XgetCIDStatus
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"


/*!
** @brief Checks the status of the specified CID. Should be called after
** calling XgetCID or XgetCIDList.
** It checks whether the requested CID is waiting to be read or still on the 
** way.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAG - Content ID of this chunk
** @param dlen - length of cDAG (currently not used)
**
** @returns 1 CID is waiting to be read
** @returns 0 waiting for chunk response
** @returns -1 on error with errno set
*/
int XgetCIDStatus(int sockfd, char* cDAG, size_t /* dlen */)
{
	int rc;
	char statusbuf[2048];
	const char *buf = "CID request status query"; //Maybe send more useful information here.

	char buffer[2048];

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket", sockfd);
		return -1;
	}
	
	if (!cDAG) {
		LOG("cDAG is null!");
		errno = EFAULT;
		return -1;
	}

	strcpy(statusbuf, "WAITING");

	// protobuf message
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCIDSTATUS);

	xia::X_Getcidstatus_Msg *x_getcidstatus_msg = xsm.mutable_x_getcidstatus();
  
  	x_getcidstatus_msg->set_numcids(1);
	x_getcidstatus_msg->set_cdaglist(cDAG);
	x_getcidstatus_msg->set_status_list(statusbuf);
	x_getcidstatus_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_data(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, buffer, sizeof(buffer))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(buffer);

	if (xia_socket_msg1.type() == xia::XGETCIDSTATUS) {
		
		xia::X_Getcidstatus_Msg *x_getcidstatus_msg1 = xia_socket_msg1.mutable_x_getcidstatus();
		strcpy(statusbuf,  x_getcidstatus_msg1->status_list().c_str()); 
		    
		if (strcmp(statusbuf, "WAITING") == 0)
			return WAITING_FOR_CHUNK;
		    
		else if (strcmp(statusbuf, "READY") == 0)
			return READY_TO_READ;
		    	
		else if (strcmp(statusbuf, "FAILED") == 0)
			return REQUEST_FAILED;
	}
	
	return REQUEST_FAILED;
}
