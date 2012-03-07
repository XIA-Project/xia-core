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
** @file Xgetsocketidlist.c
** @brief implements Xgetsocketidlist
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*!
** @brief Retrieve a list of open Xsockets.
**
** @param sockfd The control socket
** @param socket_list On output, contains a list of Xsocket ids that
** click knows about
**
** @returns the number of sockets added to the list
** @returns -1 on error with errno set
**
** FIXME: this should take a length so we know how big socket_list is
*/
int Xgetsocketidlist(int sockfd, int *socket_list)
{
	int rc;
	char buf[MAXBUFLEN];

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		LOGF("%d is not a valid Xsocket", sockfd);
		errno = EBADF;
		return -1;
	}
	
	if (!socket_list) {
		LOG("socket_list pointer is null!\n");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETSOCKETIDLIST);

	// send the protobuf containing the user data to click
	if ((rc = click_control(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_reply(sockfd, buf, sizeof(buf) - 1)) < 0) {
		LOGF("Error retreiving data from Click: %s", strerror(errno));
		return -1;
	}

	xsm.Clear();
	xsm.ParseFromString(buf);

	if (xsm.type() != xia::XGETSOCKETIDLIST) {
		LOGF("Expected type %d, got %d", xia::XGETSOCKETIDLIST, xsm.type());
		return -1; 
	}

	xia::X_Getsocketidlist_Msg *x_getsocketidlist_msg = xsm.mutable_x_getsocketidlist();
	int num_sockets = x_getsocketidlist_msg->size();

	for(int i = 0; i < num_sockets; i++) {
		socket_list[i] = x_getsocketidlist_msg->id(i);
	}

	return num_sockets;
}
