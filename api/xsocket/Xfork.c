/*
** Copyright 2015 Carnegie Mellon University
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
** @file Xfork.c
** @brief Xfork() - create a child process
*/

#include "Xsocket.h"
/*! \cond */
#include <sys/select.h>
#include <sys/poll.h>
#include <errno.h>
#include <unistd.h>
#include "Xinit.h"
#include "Xutil.h"
#include "state.h"
/*! \endcond */

static int makeList(bool increment)
{
	SocketMap *socketmap = SocketMap::getMap();
	SMap *sockets = socketmap->smap();
	SMap::iterator it;
	int rc = 0;
	int sock = 0;
	int count = 0;
	int seq = 0;
	xia::XSocketMsg xsm;

	xsm.set_type(xia::XFORK);
	xsm.set_sequence(seq);

	xia::X_Fork_Msg *fm = xsm.mutable_x_fork();

	socketmap->lock();
	//LOGF("XFORK: count = %lu\n", sockets->size());

	for (it = sockets->begin(); it != sockets->end(); it++) {
		unsigned id = it->second->getID();

		fm->add_ids(id);
		count++;

		//LOGF("adding socket:%d id:%d", sock, id);
	}
	socketmap->unlock();

	if (count) {
		fm->set_increment(increment);
		fm->set_count(count);

		sock = MakeApiSocket(SOCK_DGRAM);

		//LOG("sending socket list to click");
		if ((rc = click_send(sock, &xsm)) < 0) {
			LOGF("Error talking to Click: %s", strerror(errno));
			goto done;
		}

		// process the reply from click
		if ((rc = click_status(sock, seq)) < 0) {
			LOGF("Error getting status from Click: %s", strerror(errno));
		}
	}

done:
	int eno = errno;
	if (sock > 0) {
		freeSocketState(sock);
		(_f_close)(sock);
	}
	errno = eno;
	return rc;
}


/*!
** @brief Creates a new process by duplicating the calling process.
**
** This function is required to be used rather than the standard fork()
** call when uisng Xsockets. Because all of the XIA code resides in user
** space, we don't get the correct behavior from systems calls that affect
*** the kernel. Xfork() wraps the system fork() call so that we can we can
** maintain correct internal socket state. This prevents issues such as
** calling Xclose() in the child process also closing the same socket in the
** parent process.
**
** @note See the man page for the standard fork() call for more details.
*
** @returns On success, the PID of the child process is returned in the
** parent, and 0 is returned in the child.
** @returns -1 on failure to the parent. No child process is created,
** and errno is set appropriately.
*/
int Xfork(void)
{
	int rc = -1;

	//LOG("incrementing refcounts");
	if ((rc = makeList(true)) >= 0) {

		//LOG("Calling real fork");
		rc = (_f_fork)();
		if (rc < 0) {
			int e = errno;
			LOG("fork failed, decrementing refcounts");

			// fork failed, so don't bother to check return value here
			makeList(false);
			errno = e;

		} else if (rc == 0) {
			// reset the select socket id so the child generates a new one
			_select_fd = -1;
		}

	} else {
		// can't talk to click for some reason

		rc = -1;
	}

	return rc;
}
