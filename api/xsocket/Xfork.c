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
** @file Xselect.c
** @brief implements Xselect() and Xpoll()
*/
#include <sys/select.h>
#include <sys/poll.h>
#include <errno.h>
#include <unistd.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "state.h"

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
	//LOGF("XFORK: count = %d\n", sockets->size());

	for (it = sockets->begin(); it != sockets->end(); it++) {
		int sock = it->first;

		// find the port number associated with this Xsocket
		struct sockaddr_in sin;
		socklen_t slen = sizeof(sin);
		(_f_getsockname)(sock, (struct sockaddr*)&sin, &slen);

		fm->add_ports(sin.sin_port);
		count++;

		//LOGF("adding socket:%d port:%d", sock, sin.sin_port);
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
		}

	} else {
		// can't talk to click for some reason

		rc = -1;
	}

	return rc;
}
