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
** @file Xsetsockopt.c
** @brief implements Xsetsockopt() and Xgetsockopt()
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>
#include <fcntl.h>

/*!
** @brief Xsocket implemention of the standard setsockopt function.
**
** Xsetsockopt is used to set options on the underlying Xsocket in
** the Click layer. It does not affect the actual socket passed in which
** used by the API to communicate with Click.
**
** Supported Options:
**	\n XOPT_HLIM	Sets the 'hop limit' (hlim) element of the XIA header to the
**		specified integer value. (Default is 250)
**	\n XOPT_NEXT_PROTO Sets the next proto field in the XIA header
**
** @param sockfd	The control socket
** @param optname	The socket option to set
** @param optval	A pointer to the value to set
** @param optlen	The length of the option being set
**
** @returns 0 on success
** @returns -1 on error with errno set
** @returns errno values:
** @returns EBADF: The socket descriptor is invalid
** @returns EINVAL: Either optval is null, or optlen is invalid, or optval is out of range
** @returns ENOPROTOOPT: the specified optname is not recognized
*/
int Xsetsockopt(int sockfd, int optname, const void *optval, socklen_t optlen)
{
	int rc;
	xia::XSocketCallType type;

	/* TODO: we may need to check the type of the socket at some point, but for now
	** treat them all the same as far as options go.
	*/
	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSETSOCKOPT);
	xia::X_Setsockopt_Msg *msg = xsm.mutable_x_setsockopt();
	msg->set_opt_type(optname);

	switch (optname) {
		case XOPT_HLIM:
		{
			if (!optval || optlen != sizeof(int)) {
				errno = EINVAL;
				return -1;
			}

			int hlim = *(const int *)optval;

			if (hlim < 0 || hlim > 255) {
				LOGF("HLIM (%d) out of range", hlim);
				errno = EINVAL;
				return -1;
			}
			msg->set_int_opt(hlim);
			break;
		}
		case XOPT_NEXT_PROTO:
		{
			if (!optval || optlen != sizeof(int)) {
				errno = EINVAL;
				return -1;
			}

			int next = *(const int *)optval;

			if (next != XPROTO_XCMP) {
				LOGF("Invalid next protocol specified (%d)", next);
				errno = EINVAL;
				return -1;
			}
			msg->set_int_opt(next);
			break;
		}

		default:
			errno = ENOPROTOOPT;
			return -1;
	}

	int flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, flags &= ~O_NONBLOCK);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));

	} else if ((rc = click_reply2(sockfd, &type) ) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
	}

	fcntl(sockfd, F_SETFL, flags);

	return rc;
}

/*!
** @brief Xsocket implemention of the standard getsockopt function.
**
** Xgetsockopt is used to retrieve the settings of the underlying Xsocket
** in the Click layer. It does not access the settings of the actual
** socket passed in which is used by the API to communicate with Click.
**
** Supported Options:
**	\n XOPT_HLIM	Retrieves the 'hop limit' element of the XIA header as an integer value
**	\n XOPT_NEXT_PROTO Gets the next proto field in the XIA header
**
** @param sockfd	The control socket
** @param optname	The socket option to set (currently must be IP_TTL)
** @param optval	A pointer to the value to retrieve
** @param optlen	A pointer to the length of optval. On input this
**	should be set to the length of the data passed in. If optlen is not
**	large enough, Xgetsockopt will set it to the size needed and return
**	with errno set to EINVAL. If larger than needed, Xgetsockopt will set
**	it to the actual length of the returned data.
**
** @returns  0 on success
** @returns -1 on error with errno set
** @returns errno values:
** @returns EBADF: The socket descriptor is invalid
** @returns EINVAL: Either optval is null, or optlen is invalid, or optval is out of range
** @returns ENOPROTOOPT: the specified optname is not recognized
*/
int Xgetsockopt(int sockfd, int optname, void *optval, socklen_t *optlen)
{
	char buf[MAXBUFLEN];

	/* TODO: we may need to check the type of the socket at some point, but for now
	** treat them all the same as far as options go.
	**
	** Should we add a validate socket function that takes the expected type?
	*/
	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	if (!optval || !optlen) {
		errno = EINVAL;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETSOCKOPT);
	xia::X_Getsockopt_Msg *msg = xsm.mutable_x_getsockopt();
	msg->set_opt_type(optname);

	switch (optname) {
		case SO_ERROR:
		{
			if (*optlen < sizeof(int)) {
				*optlen = sizeof(int);
				errno = EINVAL;
				return -1;
			}

			*(int *)optval = getError(sockfd);
			break;
		}

		case XOPT_HLIM:
		{
			if (*optlen < sizeof(int)) {
				*optlen = sizeof(int);
				errno = EINVAL;
				return -1;
			}

			int flags = fcntl(sockfd, F_GETFL);
			fcntl(sockfd, F_SETFL, flags &= ~O_NONBLOCK);

			if (click_send(sockfd, &xsm) < 0) {
				LOGF("Error talking to Click: %s", strerror(errno));
				fcntl(sockfd, F_SETFL, flags);
				return -1;
			}

			xia::XSocketMsg reply;
			if (click_reply(sockfd, xia::XGETSOCKOPT, buf, sizeof(buf)) < 0) {
				LOGF("Error getting status from Click: %s", strerror(errno));
				fcntl(sockfd, F_SETFL, flags);
				return -1;
			}

			fcntl(sockfd, F_SETFL, flags);

			xsm.Clear();
			xsm.ParseFromString(buf);
			xia::X_Getsockopt_Msg *msg = xsm.mutable_x_getsockopt();

			int hlim = msg->int_opt();
			*optlen = sizeof(int);
			*(int *)optval = hlim;
			break;
		}

		case XOPT_NEXT_PROTO:
		{
			if (*optlen < sizeof(int)) {
				*optlen = sizeof(int);
				errno = EINVAL;
				return -1;
			}

			int flags = fcntl(sockfd, F_GETFL);
			fcntl(sockfd, F_SETFL, flags &= ~O_NONBLOCK);

			if (click_send(sockfd, &xsm) < 0) {
				fcntl(sockfd, F_SETFL, flags);
				LOGF("Error talking to Click: %s", strerror(errno));
				return -1;
			}

			xia::XSocketMsg reply;
			if (click_reply(sockfd, xia::XGETSOCKOPT, buf, sizeof(buf)) < 0) {
				fcntl(sockfd, F_SETFL, flags);
				LOGF("Error getting status from Click: %s", strerror(errno));
				return -1;
			}

			fcntl(sockfd, F_SETFL, flags);

			xsm.Clear();
			xsm.ParseFromString(buf);
			xia::X_Getsockopt_Msg *msg = xsm.mutable_x_getsockopt();

			int nxt = msg->int_opt();
			*optlen = sizeof(int);
			*(int *)optval = nxt;
			break;
		}

		default:
			errno = ENOPROTOOPT;
			return -1;
	}

	return 0;
}

