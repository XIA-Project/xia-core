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
** @file Xbind.c
** @brief implements Xbind
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

/*!
** @brief Bind an Xsocket to a DAG.
**
** Causes click to tear down the underlying XIA socket and also closes the UDP
** socket used to talk to click.
**
** @param sockfd	The control socket
** @param sDAG		The source service (local) DAG
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int Xbind(int sockfd, char* Sdag)
{
	xia::XSocketCallType type;
	int rc;

	if (!Sdag) {
		LOG("Sdag is NULL!");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XBIND);

	xia::X_Bind_Msg *x_bind_msg = xsm.mutable_x_bind();
	x_bind_msg->set_sdag(Sdag);

	if ((rc = click_control(sockfd, &xsm)) < 0)
		return -1;

	// process the reply from click
	rc = click_reply2(sockfd, &type);

	if (type != xia::XBIND) {
		// something bad happened
		LOGF("Expected type %d, got %d", xia::XBIND, type);
		// what do we do in this case?
	}

	// if rc is negative, errno will be set with an appropriate error code
	return rc;
}
