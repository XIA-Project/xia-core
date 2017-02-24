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

int XupdateDefaultInterface(int sockfd, int interface)
{
	int rc;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XUPDATEDEFIFACE);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	xia::X_UpdateDefIface_Msg *x_updatedefiface=xsm.mutable_x_updatedefiface();
	x_updatedefiface->set_interface(interface);

	if((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error changing default interface to: %d: %s",
				interface, strerror(errno));
		return -1;
	}
	return 0;
}


int XdefaultInterface(int sockfd) {
  	int rc = -1;
	int default_iface = -1;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XDEFIFACE);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	if((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error requesting default interface: %s", strerror(errno));
		return default_iface;
	}

	xia::XSocketMsg xsm1;
	if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
		LOGF("Error retrieving default interface: %s", strerror(errno));
		return default_iface;
	}

	if (xsm1.type() == xia::XDEFIFACE) {
		xia::X_DefIface_Msg *_msg = xsm1.mutable_x_defiface();

		default_iface = _msg->interface();
	}
	return default_iface;
}

