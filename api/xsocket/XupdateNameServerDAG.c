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

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

int XupdateNameServerDAG(int sockfd, const char *nsDAG) {
  int rc;

  if (!nsDAG) {
    LOG("new ad is NULL!");
    errno = EFAULT;
    return -1;
  }

  if (getSocketType(sockfd) == XSOCK_INVALID) {
    LOG("The socket is not a valid Xsocket");
    errno = EBADF;
    return -1;
  }

  xia::XSocketMsg xsm;
  xsm.set_type(xia::XUPDATENAMESERVERDAG);
  unsigned seq = seqNo(sockfd);
  xsm.set_sequence(seq);

  xia::X_Updatenameserverdag_Msg *x_updatenameserverdag_msg = xsm.mutable_x_updatenameserverdag();
  x_updatenameserverdag_msg->set_dag(nsDAG);

  if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  }

  // process the reply from click
  if ((rc = click_status(sockfd, seq)) < 0) {
    LOGF("Error getting status from Click: %s", strerror(errno));
    return -1;
  }

  return 0;
}


int XreadNameServerDAG(int sockfd, sockaddr_x *nsDAG) {
  	int rc = -1;

 	if (getSocketType(sockfd) == XSOCK_INVALID) {
   	 	LOG("The socket is not a valid Xsocket");
   	 	errno = EBADF;
  		return -1;
 	}

	if (!nsDAG) {
		errno = EINVAL;
		return -1;
	}

 	xia::XSocketMsg xsm;
  	xsm.set_type(xia::XREADNAMESERVERDAG);
	unsigned seq = seqNo(sockfd);
  	xsm.set_sequence(seq);

  	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  	}

	xia::XSocketMsg xsm1;
	if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	if (xsm1.type() == xia::XREADNAMESERVERDAG) {
		xia::X_ReadNameServerDag_Msg *_msg = xsm1.mutable_x_readnameserverdag();
		try {
			Graph g(_msg->dag().c_str());
			if (g.num_nodes() > 0) {
				g.fill_sockaddr(nsDAG);
				rc = 0;
			}
		} catch (std::exception e) {
			rc = -1;
			errno = EWOULDBLOCK;
		}
	}
	return rc;
}

