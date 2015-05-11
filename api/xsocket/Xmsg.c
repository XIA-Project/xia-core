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
** @file Xmsg.c
** @brief implements Xrecvmsg, Xsendmsg()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"


ssize_t Xrecvmsg(int fd, struct msghdr *msg, int flags)
{
	int rc;
	char *buf;
	int iface = 0;

	if (validateSocket(fd, XSOCK_DGRAM, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a datagram socket", fd);
		return -1;
	}

	if (flags) {
		LOGF("Flags: %s\n", xferFlags(flags));
	}

	LOGF("msghdr:\n name:%p namelen:%zu iov:%p iovlen:%zu control:%p clen:%zu flags:%08x\n",
	msg->msg_name,
	(size_t)msg->msg_namelen,
	msg->msg_iov,
	(size_t)msg->msg_iovlen,
	msg->msg_control,
	(size_t)msg->msg_controllen,
	msg->msg_flags);

	int connected = (getConnState(fd) == CONNECTED);

	if (msg == NULL || msg->msg_iov == NULL) {
		errno = EFAULT;
		return -1;
	}

	size_t size = _iovSize(msg->msg_iov, msg->msg_iovlen);

	if (msg->msg_iovlen > 1) {
		buf = (char *)malloc(size);
	} else {
		buf = (char *)msg->msg_iov[0].iov_base;
	}

	if (connected) {
		rc = _xrecvfromconn(fd, buf, size, flags, &iface);
	} else {

		sockaddr_x *sax = (sockaddr_x *)msg->msg_name;
		socklen_t *len = (sax != NULL ? &msg->msg_namelen : NULL);

		rc = _xrecvfrom(fd, buf, size, flags, sax, len, &iface);
		LOGF("returned:%d\n", rc);
	}

	if (rc > 0) {
		if (msg->msg_iovlen > 1) {

			size = _iovUnpack(msg->msg_iov, msg->msg_iovlen, buf, rc);

		} else {
			size = rc;
		}

		// FIXME: not sure this can happen
		if (size < (socklen_t)rc) {
			// set the truncated flag
			msg->msg_flags = MSG_TRUNC;
		}

		struct cmsghdr *cmsg;
		struct in_pktinfo *pinfo;

		for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {

			if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
				pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
				pinfo->ipi_ifindex = iface;
				// FIXME: remaining fields in the control msg are ignored

			} else {
				LOGF("unsupported control type %d\n", cmsg->cmsg_type);
			}
		}

	} else if (rc < 0) {
		msg->msg_flags = MSG_ERRQUEUE; // is this ok?

	} else {
		msg->msg_flags = 0;
	}

	if (msg->msg_iovlen > 1) {
		free(buf);
	}

	return rc;
}


ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
	int rc;
	size_t size;

	LOGF("fd:%d flags:%08x\n", fd, flags);
	LOGF("msghdr:\n name:%p namelen:%zu iov:%p iovlen:%zu control:%p clen:%zu flags:%08x\n",
	msg->msg_name,
	(size_t)msg->msg_namelen,
	msg->msg_iov,
	(size_t)msg->msg_iovlen,
	msg->msg_control,
	(size_t)msg->msg_controllen,
	msg->msg_flags);

	int connected = (getConnState(fd) == CONNECTED);

	if (msg == NULL || msg->msg_iov == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (getSocketType(fd) != SOCK_DGRAM) {
		errno = ENOTSOCK;
		return -1;
	}

	if (!connected && msg->msg_name == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (msg->msg_control != NULL) {
		LOG("XIA unable to handle control info on sendmsg.");
	}

	// let's try to send this thing!
	char *buf = NULL;
	if (msg->msg_iovlen > 1) {
		size = _iovPack(msg->msg_iov, msg->msg_iovlen, &buf);
	
	} else {
		size = _iovSize(msg->msg_iov, msg->msg_iovlen);
		buf = (char *)msg->msg_iov[0].iov_base;		
	}

	LOGF("sending:%zu\n", size);

	if (connected) {
		rc = send(fd, buf, size, flags);

	} else {
		rc = sendto(fd, buf, size, flags, (struct sockaddr*)msg->msg_name, msg->msg_namelen);
	}

	if (msg->msg_iovlen > 1) {
		free(buf);
	}

	return rc;
}
