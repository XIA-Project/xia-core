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
  @file Xutil.c
  @brief Impliments internal socket helper functions
*/
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

#define CONTROL 1
#define DATA 2


int validateSocket(int sock, int stype, int err)
{
	int st = getSocketType(sock);
	if (st == stype || st == XSOCK_RAW)
		return 0;
	else if (st == XSOCK_INVALID)
		errno = EBADF;
	else
		errno = err;

	return -1;
}

int click_x(int sockfd, int kind, xia::XSocketMsg *xsm)
{
	int rc;
	struct sockaddr_in sa;
	socklen_t slen;

	assert(xsm);

	// TODO: cache these so we don't have to set everything up each time we
	// are called

	slen = sizeof sa;
	sa.sin_family = PF_INET;
	if (kind == DATA) {
		sa.sin_addr.s_addr = inet_addr(CLICKDATAADDRESS);
		sa.sin_port = htons(atoi(CLICKDATAPORT));
	} else if (kind == CONTROL) {
		sa.sin_addr.s_addr = inet_addr(CLICKCONTROLADDRESS);
		sa.sin_port = htons(atoi(CLICKCONTROLPORT));
	} else {
		LOG("invalid click port specified");
		return -1;
	}

	std::string p_buf;
	xsm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, slen);

		if (rc == -1) {
			LOGF("click socket failure: errno = %d", errno);
			break;
		} else {
			remaining -= rc;
			p += rc;
			if (remaining > 0) {
				LOGF("%d bytes left to send", remaining);
#if 1
				// FIXME: click will crash if we need to send more than a 
				// single buffer to get the entire block of data sent. Is 
				// this fixable, or do we have to assume it will always go
				// in one send?
				LOG("click can't handle partial packets");
				rc = -1;
				break;
#endif
			}
		}	
	}

	return  (rc >= 0 ? 0 : -1);
}

int click_data(int sockfd, xia::XSocketMsg *xsm)
{
	return click_x(sockfd, DATA, xsm);
}

int click_control(int sockfd, xia::XSocketMsg *xsm)
{
	return click_x(sockfd, CONTROL, xsm);
}

int click_reply(int sockfd, char *buf, int buflen)
{
	struct sockaddr_in sa;
	socklen_t len;
	int rc;

	len = sizeof sa;

	memset(buf, 0, buflen);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		LOGF("error(%d) getting reply data from click", errno);
		return -1;
	}

	return rc;
}

int click_reply2(int sockfd, xia::XSocketCallType *type)
{
	char buf[1024];
	unsigned buflen = sizeof(buf);
	struct sockaddr_in sa;
	socklen_t len;
	int rc;

	len = sizeof sa;

	memset(buf, 0, buflen);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		LOGF("error(%d) getting reply data from click", errno);
		return -1;
	}

	xia::XSocketMsg reply;
	reply.ParseFromString(buf);
	xia::X_Result_Msg *msg = reply.mutable_x_result();

	*type = msg->type();

	rc = msg->return_code();
	if (rc == -1)
		errno = msg->err_code();

	return rc;
}
