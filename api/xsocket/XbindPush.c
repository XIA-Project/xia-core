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
** @brief implements XbindPush()
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>
#include "dagaddr.hpp"

/*!
** @brief Bind a Chunk Xsocket to a DAG.
**
** Assign the specified DAG to to the Xsocket referred to by sockfd. The DAG's
** final intent should be a valid SID. This only works with chunk sockets.
** 
**
** An un-bound Xsocket will be given a random local SID that is currently not
** available to the application.
**
** @param sockfd	The control socket
** @param addr		The source service (local) DAG
** @param addrlen	The size of addr
**
** @returns 0 on success
** @returns -1 on error with errno set to an error compatible with those
** retuned by the standard bind call.
*/
int XbindPush(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int rc;

	if (addrlen == 0) {
		errno = EINVAL;
		return -1;
	}

	if (!addr) {
		LOG("addr is NULL!");
		errno = EFAULT;
		return -1;
	}

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	Graph g((sockaddr_x*)addr);
	if (g.num_nodes() <= 0) {
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XBINDPUSH);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_BindPush_Msg *x_bindpush_msg = xsm.mutable_x_bindpush();
	x_bindpush_msg->set_sdag(g.dag_string().c_str());

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_status(sockfd, seq)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
	}

	// if rc is negative, errno will be set with an appropriate error code
	return rc;
}
