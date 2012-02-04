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
#include "errno.h"

/*!
** @brief Sends data to a remote socket. Xconnect must be called before
** using this function.
**
** @param sockfd - The socket to send the data on
** @param buf - the data to send
** @param len - lenngth of the data to send @NOTE: currently the
** Xsendto api is limited to sending at most 1024 bytes.
** @param flags - (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
**
** @returns number of bytes sent on success
** @returns -1 on failure with errno set.
**
** FIXME: what should we return if we need to look and some data is sent, 
** but a subsequent internal send errors out? Using the number of bytes 
** already sent. It doesn't feel right though.
*/

int Xsend(int sockfd, const void *buf, size_t len, int /*flags*/)
{
	xia::XSocketCallType type;
	int rc;
	int count;
	int sent = 0;
	const char *p = (const char *)buf;

	if (len == 0)
		return 0;

	if (!buf) {
		LOG("buffer pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSEND);

	xia::X_Send_Msg *x_send_msg = xsm.mutable_x_send();

	// we can only send MAX_DGRAM bytes at a time, so will need to loop
	// if the user requested a send with a larger size
	while (len) {
		count = MIN(len, MAX_DGRAM);
		LOGF("sending %d bytes %d remaining\n", count, len - count);
		x_send_msg->set_payload((const char*)p, count);

		// send the protobuf containing the user data to click
		if ((rc = click_data(sockfd, &xsm)) < 0)
			break;

		// process the reply from click
		if((rc = click_reply2(sockfd, &type)) < 0)
			break;

		if (type != xia::XSEND) {
			LOGF("Expected type %d, got %d", xia::XSEND, type);
			// what do we do in this case?
			// we probably sent the data, but can't be sure
		}
		sent += count;
		len -= count;
		p += count;
	}

	if (sent)
		return sent;

	else if (rc < 0) {
		// if negative, errno will be set with an appropriate error code
		return -1;
//	}else if (rc == 0) {
//		// everything went fine, tell caller we sent the whole buffer
//		return len;
	}

	// not sure we'll ever get here
	return rc;
}
