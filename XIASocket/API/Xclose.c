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
** @file Xclose.c
** @brief implements Xclose
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

/*!
** @brief Close an Xsocket.
**
** Causes click to tear down the underlying XIA socket and also closes the UDP
** socket used to talk to click.
**
** @param sockfd	The control socket
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int Xclose(int sockfd)
{
	xia::XSocketCallType type;
	int xerr = 0;
	int rc;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XCLOSE);
	xia::X_Close_Msg *x_close_msg = xsm.mutable_x_close();

	// FIXME: eliminate or make this a debug time option, 
	// it doesn't do anything
	const char *message="close socket";
	x_close_msg->set_payload(message);

	if ((rc = click_control(sockfd, &xsm)) < 0) {
		// print an error message
		xerr = ECLICKCONTROL;
	} else {
		rc = click_reply2(sockfd, &type);
	}

	close(sockfd);

	if (xerr != 0)
		errno = ECLICKCONTROL;
	return rc;
}

