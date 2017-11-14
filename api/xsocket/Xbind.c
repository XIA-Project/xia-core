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
** @brief Xbind() - bind a name to a socket
*/

#include "Xsocket.h"
/** @cond */
#include "Xinit.h"
#include "Xutil.h"
#include "Xkeys.h"
#include <errno.h>
#include "dagaddr.hpp"
/** @endcond */

/*!
** @brief bind a DAG to an Xsocket
**
** Assign the specified DAG to to the Xsocket referred to by sockfd. The DAG's
** final intent should be a valid SID.
**
** @param sockfd	The socket
** @param addr		sockaddr_x structure containing the DAG to be assigned
** @param addrlen	The size of addr
**
** @note See the man page for the standard bind() call for more details.
**
** @returns 0 on success
** @returns -1 on error with errno set to an error compatible with those
** retuned by the standard bind call.
*/
int Xbind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	int rc;
	int transport_type;

	if (addrlen == 0) {
		errno = EINVAL;
		return -1;
	}

	if (!addr) {
		LOG("addr is NULL!");
		errno = EFAULT;
		return -1;
	}

	transport_type = getSocketType(sockfd);
	if (transport_type == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	Graph g((sockaddr_x*)addr);
	if (g.num_nodes() <= 0) {
		errno = EINVAL;
		return -1;
	}

	// Verify access to keys for the user provided SID (SOCK_STREAM only)
	if (transport_type == SOCK_STREAM) {
		// Extract source SID from g
		std::string intent_type = g.get_final_intent().type_string();
//		LOGF("Xbind: Intent type:%s:", intent_type.c_str());
		if(intent_type.compare("SID") != 0) {
			LOGF("ERROR: Final intent %s is not SID", intent_type.c_str());
			errno = EINVAL;
			return -1;
		}
		std::string intent = g.get_final_intent().to_string();
//		LOGF("Xbind: Intent:%s:", intent.c_str());
		// Stat <keydir>/<SID>{,.pub}
		if(!XexistsSID(intent.c_str())) {
			LOGF("ERROR: Keys for SID:%s not found", intent.c_str());
			errno = EINVAL;
			return -1;
		}

		// This socket has a valid user provided SID with keys
		setSIDAssigned(sockfd);
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XBIND);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Bind_Msg *x_bind_msg = xsm.mutable_x_bind();
	x_bind_msg->set_sdag(g.dag_string().c_str());

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
