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

/*
* send like datagram sending function for XIA
* IMPORTANT: Works for datagrams only
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "errno.h"

int Xsend(int sockfd, const void *buf, size_t len, int /*flags*/)
{
	xia::XSocketCallType type;
	int rc;

	if (len == 0)
		return 0;

	if (!buf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	// FIXME: validate socket

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSEND);

	xia::X_Send_Msg *x_send_msg = xsm.mutable_x_send();
	x_send_msg->set_payload((const char*)buf, len);

	// send the protobuf containing the user data to click
	if ((rc = click_data(sockfd, &xsm)) < 0)
		return -1;

	// process the reply from click
	rc = click_reply2(sockfd, &type);

	if (type != xia::XSEND) {
		// something bad happened
		LOGF("Expected type %d, got %d", xia::XSEND, type);
		// what do we do in this case?
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
