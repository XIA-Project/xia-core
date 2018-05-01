/* ts=4 */
/*
** Copyright 2012 Carnegie Mellon University
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
 @file Xgethostname.c
 @brief Xgethostname() - get XIA host name
*/

#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include <netdb.h>
#include "minIni.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
#include <stdlib.h>
#include <sys/types.h>
#include <ifaddrs.h>
/*! \endcond */

/*!
** @brief get the XIA host name
**
* * Returns the XIA host name in a null-terminated string.
**
** @param name the destination for the host name. On exit name
** contains a null terminated string. It will be truncated if
** there is not enough room.
** @param len the length of name
**
** @returns  0 on success
** @returns -1 on failure with errno is set appropriately.
*/
int Xgethostname(char *name, size_t len)
{
	// TODO: Set errno for various errors
	int rc;

	// A socket that Click can associate us with
	int sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		LOG("Xgethostname: unable to create Xsocket");
		return -1;
	}

	// Send a request to Click to read info on all interfaces
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETHOSTNAME);
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

	// Send list of interfaces up to the application
	if (xsm1.type() == xia::XGETHOSTNAME) {
		xia::X_GetHostName_Msg *msg = xsm1.mutable_x_gethostname();
		std::string hostname = msg->hostname();
		if(len < hostname.length()) {
			LOGF("Hostname truncated: actual: %d, returned %d\n", (int)hostname.length(), (int)len);
		}
		strncpy(name, hostname.c_str(), len);
		rc = 0;
	} else {
		LOG("Xgethostname: ERROR: Invalid response for XGETHOSTNAME request");
		rc = -1;
	}
	return rc;
}
