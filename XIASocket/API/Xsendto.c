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
* sendto like datagram sending function for XIA
*/

//FIXME: add note indicating that we expect the send to be atomic, and that
// then entire buffer is sent, and not just a partial one.

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

/* dDAG is a NULL terminated string */
int Xsendto(int sockfd,const void *buf, size_t len, int /*flags*/,
		char* dDAG, size_t /*dlen*/)
{
	xia::XSocketCallType type;
	int rc;

	// FIXME: validate the socket!

	if (len == 0)
		return 0;

	if (!buf || !dDAG) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}

	/* === New version
	 * Now, dDAG and buf are contained in the google protobuffer message (encapsulated within UDP),
	 * then passed to the Click UDP.
	 */

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSENDTO);

	xia::X_Sendto_Msg *x_sendto_msg = xsm.mutable_x_sendto();
	x_sendto_msg->set_ddag(dDAG);
	x_sendto_msg->set_payload((const char*)buf, len);

	if ((rc = click_data(sockfd, &xsm)) < 0)
		return -1;

	// process the reply from click
	rc = click_reply2(sockfd, &type);

	if (type != xia::XSENDTO) {
		// something bad happened
		LOGF("Expected type %d, got %d", xia::XSENDTO, type);
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
