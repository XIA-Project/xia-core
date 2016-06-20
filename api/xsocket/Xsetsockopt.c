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

int ssoCheckSize(unsigned *size, unsigned needed)
{
	if (*size < needed) {
		*size = needed;
		errno = EINVAL;
		return -1;
	}

	// set to actual size of returned data
	*size = needed;
	return 0;
}

/*
** get an integer sized parameter from click
*/
int ssoGetInt(int sockfd, int optname, int *optval, socklen_t *optlen)
{
	if (ssoCheckSize(optlen, sizeof(int)) < 0)
		return -1;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETSOCKOPT);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);
	xia::X_Getsockopt_Msg *msg = xsm.mutable_x_getsockopt();
	msg->set_opt_type(optname);

	if (click_send(sockfd, &xsm) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	xsm.Clear();
	if (click_reply(sockfd, seq, &xsm) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	msg = xsm.mutable_x_getsockopt();
	*optval = msg->int_opt();
	*optlen = sizeof(int);

	// FIXME: get return code from protobuf
	return 0;
}

/*
** get an integer sized parameter from click
*/
int ssoPutInt(int sockfd, int optname, const int *optval, socklen_t optlen)
{
	if (ssoCheckSize(&optlen, sizeof(int)) < 0)
		return -1;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XSETSOCKOPT);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);
	xia::X_Setsockopt_Msg *msg = xsm.mutable_x_setsockopt();
	msg->set_opt_type(optname);
	msg->set_int_opt(*optval);

	if (click_send(sockfd, &xsm) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if (click_status(sockfd, seq) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	return 0;
}

int ssoGetParam(int param, void *optval, socklen_t *optlen)
{
	if (ssoCheckSize(optlen, sizeof(int)) < 0) {
		return -1;
	}

	*(int *)optval = param;
	*optlen = sizeof(int);
	return 0;
}

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
	int rc = 0;

	/* TODO: we may need to check the type of the socket at some point, but for now
	** treat them all the same as far as options go.
	*/
	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	if (!optval) {
		errno = EINVAL;
		return -1;
	}
	switch (optname) {

		// send these values to click *******************************

		case XOPT_HLIM:
		{
			int hlim = *(const int *)optval;

			if (hlim < 0 || hlim > 255) {
				LOGF("HLIM (%d) out of range", hlim);
				errno = EINVAL;
				rc = -1;
			} else {
				rc = ssoPutInt(sockfd, optname, (const int *)optval, optlen);
			}
			break;
		}

		case XOPT_NEXT_PROTO:
		{
			int next = *(const int *)optval;

			if (next != XPROTO_XCMP) {
				LOGF("Invalid next protocol specified (%d)", next);
				errno = EINVAL;
				rc = -1;
			} else {
				rc = ssoPutInt(sockfd, optname, (const int *)optval, optlen);
			}
			break;
		}

		case XOPT_BLOCK:
		{
			rc = ssoPutInt(sockfd, optname, (const int *)optval, optlen);
			break;
		}

		// firefox wants to set this for some reason???
		case SO_ERROR:
			if (ssoCheckSize(&optlen, sizeof(int)) < 0) {
				rc = -1;
			}
			else {
				rc = ssoPutInt(sockfd, optname, (const int *)optval, optlen);
			}
			break;

		// this is handled in the API & in click ********************
		case SO_DEBUG:
			if (ssoCheckSize(&optlen, sizeof(int)) < 0) {
				rc = -1;
			}
			else {
				setDebug(sockfd, *(int *)optval);
				rc = ssoPutInt(sockfd, optname, (const int *)optval, optlen);
			}
			break;


		// SHOIULD IMPLEMENT! ***************************************
		// FIXME: implement these!
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		case SO_LINGER:
		case SO_REUSEADDR:
			LOGF("Uh Oh, we need support for %s in XIA\n", optValue(optname));
			rc = 0;
			break;

		// FIXME: MAY NEED ******************************************

		case SO_BROADCAST:
			// FIXME: can we just go ahead and mark this as success?
			// or do we need to do something to check to see if bradcast DAGS are allowed on this socket?
			if (getSocketType(sockfd) == SOCK_DGRAM) {
				rc = -1;
				errno = ENOPROTOOPT;
				break;
			}
			// else silently ignore
			break;

		case SO_RCVLOWAT:
		case SO_SNDLOWAT:
			// Probably will never need to support this
			// return the default linux value of 1
			rc = 0;
			break;


		// CAN SAFELY IGNORE ****************************************

		case SO_KEEPALIVE:
			// we don't have keepalive so lie and say we did it
			rc = 0;
			break;

		case SO_BSDCOMPAT:
			// safe to mark as unsupported, being phased out on linux anyway
			errno = ENOPROTOOPT;
			rc = -1;
			break;


		// OK to lie about???
		case SO_DONTROUTE:
			rc = 0;
			break;

		// READ ONLY OPTIONS ****************************************
		case SO_ACCEPTCONN:
		case SO_DOMAIN:
		case SO_PROTOCOL:
		case SO_TYPE:
			LOGF("Option %s is read only\n", optValue(optname));
			errno = ENOPROTOOPT;
			rc = -1;
			break;


		// NOT SUPPORTED/DOESN"T MAKE SENSE IN XIA ******************
		case SO_BINDTODEVICE:	// should we support this one when we get multihoming?
		case SO_MARK:
		case SO_OOBINLINE:
		case SO_PASSCRED:
		case SO_PEERCRED:
		case SO_PRIORITY:
		case SO_RCVBUFFORCE:
		case SO_SNDBUFFORCE:
		case SO_TIMESTAMP:
		default:
			// return generic error
			LOGF("Option %s not supported in XIA\n", optValue(optname));
			errno = ENOPROTOOPT;
			rc = -1;
	}

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
**	\n SO_TYPE 		Returns the type of socket (SOCK_STREAM, etc...)
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
	int rc = 0;

	if (getSocketType(sockfd) == XSOCK_INVALID) {
		errno = EBADF;
		return -1;
	}

	if (!optval || !optlen) {
		errno = EINVAL;
		return -1;
	}

	switch (optname) {

		// get these values from click ******************************
		case XOPT_HLIM:
		case XOPT_NEXT_PROTO:
		case XOPT_ERROR_PEEK:
		case SO_ACCEPTCONN:
		case SO_ERROR:
			rc = ssoGetInt(sockfd, optname, (int *)optval, optlen);
			break;

		case SO_DEBUG:
			// stored in the API & in click, return from the API
			rc = ssoGetParam(getDebug(sockfd), optval, optlen);
			break;

		case SO_DOMAIN:
			// FIXME: conver this to AF_INET in wrapper
			rc = ssoGetParam(AF_XIA, optval, optlen);
			break;

		case SO_PROTOCOL:
			rc = ssoGetParam(getProtocol(sockfd), optval, optlen);
			break;

		case SO_TYPE:
			ssoGetParam(getSocketType(sockfd), optval, optlen);
			break;


		// SHOIULD IMPLEMENT! ***************************************
		// FIXME: implement these!
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		case SO_LINGER:
			LOGF("Uh Oh, we need support for %s in XIA\n", optValue(optname));
			rc = -1;
			break;

		// FIXME: MAY NEED ******************************************

		case SO_BROADCAST:
			// FIXME: can we just go ahead and mark this as success?
			// or do we need to do something to check to see if bradcast DAGS are allowed on this socket?
			if (getSocketType(sockfd) == SOCK_DGRAM) {
				rc = -1;
				errno = ENOPROTOOPT;
				break;
			}
			// else silently ignore
			break;

		case SO_REUSEADDR:
			// just say it's not enabled
			ssoGetParam(0, optval, optlen);
			rc = 0;
			break;

		case SO_RCVLOWAT:
		case SO_SNDLOWAT:
			// Probably will never need to support this
			// return the default linux value of 1
			rc = ssoGetParam(1, optval, optlen);
			break;


		// CAN SAFELY IGNORE ****************************************

		case SO_KEEPALIVE:
			// we don't have keepalive so lie and say we do
			ssoGetParam(0, optval, optlen);
			rc = 0;
			break;

		case SO_BSDCOMPAT:
			// safe to mark as unsupported, being phased out on linux anyway
			errno = ENOPROTOOPT;
			rc = -1;
			break;

		// NOT SUPPORTED/DOESN"T MAKE SENSE IN XIA ******************
		case SO_BINDTODEVICE:	// should we support this one when we get multihoming?
		case SO_DONTROUTE:
		case SO_MARK:
		case SO_OOBINLINE:
		case SO_PASSCRED:
		case SO_PEERCRED:
		case SO_PRIORITY:
		case SO_RCVBUFFORCE:
		case SO_SNDBUFFORCE:
		case SO_TIMESTAMP:
		default:
			// return generic error
			LOGF("Option %s not supported in XIA\n", optValue(optname));
			errno = ENOPROTOOPT;
			rc = -1;
	}

	return rc;
}

