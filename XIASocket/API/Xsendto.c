/* ts=4 */
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
** @file Xsend.c
** @brief implements Xclose
*/
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

/*!
** @brief Sends a datagram to the specified DAG.
**
** @param sockfd - The socket to send the data on
** @param buf - the data to send
** @param len - lenngth of the data to send @NOTE: currently the
** Xsendto api is limited to sending at most 1024 bytes.
** @param flags - (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param dDAG address to send the datagram to
** @param dlen length of the DAG, currently unused
**
** @returns number of bytes sent on success
** @returns -1 on failure with errno set.
**
** @warning because the XIA header takes up room in the datagram, this
** function currently truncates the data to be sent to 1024 bytes. We 
** should probably add an Xgetsockopt paarameter to allow the user to 
** determine the maximum buffer size that will fit in the datagram.
*/
int Xsendto(int sockfd,const void *buf, size_t len, int /*flags*/,
		char* dDAG, size_t /*dlen*/)
{
	xia::XSocketCallType type;
	int rc;

	if (len == 0)
		return 0;
	else if (len > MAX_DGRAM)
		len = MAX_DGRAM;

	if (!buf || !dDAG) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSENDTO);

	xia::X_Sendto_Msg *x_sendto_msg = xsm.mutable_x_sendto();
	x_sendto_msg->set_ddag(dDAG);
	x_sendto_msg->set_payload((const char*)buf, len);

	if ((rc = click_data(sockfd, &xsm)) < 0)
		return -1;

	// process the reply from click
	rc = click_reply2(sockfd, &type);

	if ( rc >= 0 && type != xia::XSENDTO) {
		// something bad happened
		LOGF("Expected type %d, got %d", xia::XSENDTO, type);
		// FIXME: what do we do in this case?
	}

	if (rc < 0) {
		// if negative, errno will be set with an appropriate error code
		return -1;
	}else if (rc == 0) {
		// everything went fine, tell caller we sent the whole buffer
		return len;
	}

	// not sure we'll ever get here
	return rc;
}
