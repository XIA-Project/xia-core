/* ts=4 */
/*
** Copyright 2016 Carnegie Mellon University
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
** @file Xnotify.c
** @brief implements Xnotify()
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

/*!
** @brief Block waiting for notification that the network interface has changed state.
**
** WARNING: THIS IS A STOPGAP API THAT WILL BE REPLACED ONCE WE DETERMIN WHAT
** THE LONGERM NOTIFICATION MECHANISM LOOKS LIKE. BE PREPARED FOR THIS CALL
** TO BE DEPRECATED OR MODIFIED
**
** Blocks and waits for for click to return a status when the XHCP client daemon
** changes the AD or other network parameters. Multiple applications or threads
** may call this API, and all will be nofitied when a change occurs.
**
** Do not call Xfork while blocking on this function. Only one of the processes will
** properly recieve the notification.
**
** @returns 0 on success
** @returns -1 on error with errno set
*/
int Xnotify(void)
{
	int rc = -1;
	int sock = 0;

	xia::XSocketMsg xsm;
	//xia::X_Notify_Msg *xnm;
	xsm.set_type(xia::XNOTIFY);
	xsm.set_sequence(0);

	sock = MakeApiSocket(SOCK_DGRAM);

	if ((rc = click_send(sock, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		goto done;

	} else if ((rc = click_reply(sock, 0, &xsm)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
	}

done:
	if (sock > 0) {
		freeSocketState(sock);
		(_f_close)(sock);
	}

	return rc;
}
