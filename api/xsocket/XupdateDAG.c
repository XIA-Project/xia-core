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
 @file XupdateDAG.c
 @brief Implements XreadLocalHostAddr()
*/
#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

#define MAX_RV_DAG_SIZE 1024

/*!
 * @brief update the DAG for an interface
 *
 * Each interface has a DAG associated with it. This API allows a user
 * space application to change the DAG for a given interface
 *
 * @param sockfd a previously allocated Xsocket
 * @param interface the interface whose DAG needs to be changed
 * @param rdag the router's DAG
 * @param r4id the router's 4ID
 *
 * @returns -1 on failure
 * @returns 0 on success
 */
int XupdateDAG(int sockfd, int interface, const char *rdag, const char *r4id,
		bool is_router) {
  int rc;

  if (!rdag) {
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
  xsm.set_type(xia::XUPDATEDAG);
  unsigned seq = seqNo(sockfd);
  xsm.set_sequence(seq);

  xia::X_Updatedag_Msg *x_updatedag_msg = xsm.mutable_x_updatedag();
  x_updatedag_msg->set_is_router(is_router);
  x_updatedag_msg->set_interface(interface);
  x_updatedag_msg->set_dag(rdag);
  x_updatedag_msg->set_ip4id(r4id);

  if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
  }

  // process the reply from click
  xia::XSocketMsg xsm1;
  if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
	LOGF("Error getting status from Click: %s", strerror(errno));
	return -1;
  }
  if(xsm1.type() == xia::XUPDATEDAG) {
	  xia::X_Updatedag_Msg *msg = xsm1.mutable_x_updatedag();
	  std::string newDAG = msg->dag();
	  printf("XupdateDAG: Click updated DAG: %s\n", newDAG.c_str());
  } else {
	  LOG("XupdateDAG: ERROR: Invalid response for XUPDATEDAG");
	  return -1;
  }

  return 0;
}

/*!
 * @brief update the rendezvous service of our new address
 *
 * When we join a new network, if there is a rendezvous service associated
 * with the interface that migrated, we send a notification to the service
 * of our new address.
 *
 * \todo require feedback response from the rendezvous service control plane
 *
 * This is a one time message with no feedback. So if the message is lost
 * the rendezvous service won't be able to forward packets.
 *
 * @param sockfd a previously created XSocket
 * @param interface the interface that just joined a new network or came up
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int XupdateRV(int sockfd, int interface)
{
	int rc;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XUPDATERV);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_Updaterv_Msg *x_updaterv_msg = xsm.mutable_x_updaterv();
	x_updaterv_msg->set_interface(interface);

	if((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error asking Click transport to update RV: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/*!
** @brief retrieve the DAG and 4ID associated with this socket.
**
** The DAG and 4ID assigned by the XIA stack. This call retrieves them
** so that they can be used for creating DAGs or for other purposes in user
** applications.
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
** @param localhostDAG buffer to receive the default DAG for this host
** @param lenDAG size of the localhostDAG buffer
** @param local4ID buffer to receive 4ID for this host (if known)
** @param len4ID size of the 4ID buffer
**
** @returns 0 on success
** @returns -1 on failure with errno set
**
*/
int XreadLocalHostAddr(int sockfd, char *localhostDAG, unsigned lenDAG, char *local4ID, unsigned len4ID) {
	int rc;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOGF("The socket %d is not a valid Xsocket", sockfd);
		errno = EBADF;
		return -1;
	}

	if (localhostDAG == NULL) {
		LOG("NULL pointer!");
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREADLOCALHOSTADDR);
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

	if (xsm1.type() == xia::XREADLOCALHOSTADDR) {

		xia::X_ReadLocalHostAddr_Msg *_msg = xsm1.mutable_x_readlocalhostaddr();

		// Error out if user provided buffers are insufficient
		// We used to return truncated entries; that can cause confusion
		unsigned actualdaglen = _msg->dag().size() + 1;
		unsigned actual4idlen = _msg->ip4id().size() + 1;

		if(lenDAG < actualdaglen) {
			LOGF("ERROR: DAG buffer too short: %u, needed: %u", lenDAG, actualdaglen);
			return -1;
		}
		if(local4ID && (len4ID < actual4idlen)) {
			LOGF("ERROR: 4ID buffer too short: %u, needed: %u", len4ID, actual4idlen);
			return -1;
		}
		strncpy(localhostDAG, (_msg->dag()).c_str(), lenDAG);
		if (local4ID) {
			strncpy(local4ID, (_msg->ip4id()).c_str(), len4ID);
		}
		rc = 0;
	} else {
		LOG("XreadlocalHostAddr: ERROR: Invalid response for XREADLOCALHOSTADDR request");
		rc = -1;
	}
	return rc;

}


/*
** @brief tell if this node is an XIA-IPv4 dual-stack router
**
**
** @param sockfd an Xsocket (may be of any type XSOCK_STREAM, etc...)
**
** @returns 1 if this is an XIA-IPv4 dual-stack router
** @returns 0 if this is an XIA router
** @returns -1 on failure with errno set
**
*/
int XisDualStackRouter(int sockfd) {
	int rc;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOG("The socket is not a valid Xsocket");
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XISDUALSTACKROUTER);
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

	if (xsm1.type() == xia::XISDUALSTACKROUTER) {
		xia::X_IsDualStackRouter_Msg *_msg = xsm1.mutable_x_isdualstackrouter();
		rc = _msg->flag();
	} else {
		rc = -1;
	}
	return rc;

}
