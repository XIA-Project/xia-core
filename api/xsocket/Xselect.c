/*
** Copyright 2013 Carnegie Mellon University
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
** @brief implements Xselect()
*/
#include <sys/select.h>
#include <sys/poll.h>
#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

typedef struct {
	int fd;
	unsigned port;
} Sock2Port;

/*!
** @brief waits for one of a set of Xsockets to become ready to perform I/O.
**
** Xsocket specific version of poll. See the poll man page for more detailed information.
**
** #include <sys/poll.h>
**
** @param ufds array of pollfds indicating sockets and states to check for
** @param nfds number of entries in ufds
** \n socket ids specified as 0 or negative will be ignored
** \n valid flags for events are POLLIN | POLLOUT | POLLERR
** \n revents contains the returned flags and can be POLLIN | POLLOUT | POLLERR | POLLINVAL | POLLHUP
** @param timeout number of milliseconds to wait for an event to happen
**
** @returns 0 if timeout occured
** @returns a positive integer indicating the number of sockets with return events
** @retuns -1 with errno set if an error occured
**
** @warning this function is only valid for stream and datagram sockets. 
*/
int Xpoll(struct pollfd *ufds, unsigned nfds, int timeout)
{
	int sock;
	int actionable = 0;
	int active = 0;

	Sock2Port *s2p = (Sock2Port*)calloc(nfds, sizeof(Sock2Port));
	// FIXME check for error here
	// FIXME make sure we free s2p if we exit early

	// if timeout is 0, don't do anything
	if (timeout == 0)
		return 0;

	// it's possible to call with no ufds and just do a timeout
	// so make sure if ufds is set that we know how many there are
	if (ufds == NULL && nfds > 0) {
		errno = EFAULT;
		return -1;
	}

	// see how many sockets in the set actually want poll results back
	for (unsigned i = 0; i < nfds; i++) {
		if (ufds[i].fd > 0 && (ufds[i].events & (POLLIN | POLLOUT | POLLPRI)))
			actionable ++;
	}

	if (!actionable) {
		// just sleep for the specified time
		if (timeout < 0) {
			// ERROR this would block forever
			errno = EINVAL;
			return -1;
		}
		usleep(timeout * 1000);
		return 0;
	}
	
	actionable = 0;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPOLL);
	xsm.set_sequence(0);

	xia::X_Poll_Msg *pollMsg = xsm.mutable_x_poll();
	pollMsg->set_timeout(timeout);
	pollMsg->set_nfds(nfds);

	for (unsigned i = 0; i < nfds; i++) {
		xia::X_Poll_Msg::PollFD *pfd = pollMsg->add_pfds();

		pfd->set_flags(ufds[i].events);

		// click needs the port, so look it up from socket
		if (ufds[i].fd < 0) {

			// printf("port is negative, we'll skip it\n");
			pfd->set_port(0);
			pfd->set_flags(0);
			ufds[i].revents = 0;

		// FIXME, we should probably only let this work for stream and datagram sockets
		} else if (getSocketType(ufds[i].fd) == XSOCK_INVALID) {
			//printf("socket %d is not a valid Xsocket\n", ufds[i].fd);

			ufds[i].revents = POLLNVAL;
			actionable++;

		} else {
			active ++;

			ufds[i].revents = 0;

			// find the port number associated with this Xsocket
			struct sockaddr_in sin;
			socklen_t slen = sizeof(sin);
			getsockname(ufds[i].fd, (struct sockaddr*)&sin, &slen);
			//printf("sock %d, port %d, flags %x\n", ufds[i].fd, ntohs(sin.sin_port), ufds[i].events);

			pfd->set_port(sin.sin_port);
			s2p[i].fd = ufds[i].fd;
			s2p[i].port = sin.sin_port;
		}


	}

	if (actionable) {
		// we hit an error condition, return results immediately
		return actionable;

	}

	if (active == 0) {
		// no sockets need to be checked, just do a timeout?
		if (timeout > 0)
			usleep(timeout * 1000);
		return 0;
	}

	// because this isn't a socket from the suer, we will just block until it returns
	// so don't set to nonblocking
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("error creating Xpoll socket: %s", strerror(errno));
		return -1;
	}

	click_send(sock, &xsm);

	int rc = click_reply(sock, 0, &xsm);
	close(sock);

	// FIXME check for errors here

	xia::X_Poll_Msg *pout = xsm.mutable_x_poll();

	rc = pout->nfds();

	if (rc == 0) {
		// timeout occurred
	
	} else if (rc < 0) {
		// error occurred
	
	} else {
		// we got status back

		// loop thru result mapping port back onto socket
		for (int i = 0; i < rc; i++) {
				const xia::X_Poll_Msg::PollFD& pfd_out = pout->pfds(i);
				unsigned port = pfd_out.port();
				unsigned flags = pfd_out.flags();
				
				//printf("poll returned x%0x for %d\n", flags, port);

				int fd = 0;
				for (int j = 0; j < nfds; j++) {
					if (port == s2p[j].port) {
						fd = s2p[j].fd;
						break;
					}
				}

				for (int j = 0; j < nfds; j++) {
					if (ufds[j].fd == fd) {
						ufds[j].revents = flags;
						break;
					}
				}
		}
	}

	return rc;
}



/*!
** @brief Perform a minimal select
**
** HACK!!! This is a stub to fake select. Xpoll should be used instead.
**
** @param ndfs The highest socket number contained in the fd_sets (ignored)
** @param readfds fd_set containing sockets to check for readability (ignored)
** @param writefds fd_set containing sockets to check for writability (ignored)
** @param errorfds fd_set containing sockets to check for errors (ignored)
** @param timeout amount of time to wait for a socket to change state (ignored)
** @returns This stub always returns 1
**
** @warning This is a stub and does not actually perform a select. Use Xpoll instead
**
*/
int Xselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout)
{
	UNUSED(nfds);
	UNUSED(readfds);
	UNUSED(writefds);
	UNUSED(errorfds);
	UNUSED(timeout);
	// fake stub to make apps that need select happy-ish until we get select implemented for XIA

	// assumes that user is asking for a single socket, tell them that it is ready even though we
	// don't know. Current code will block so it's sorta OK for now.
	return 1;
}
