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

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>

#define CONTROL 1
#define DATA 2

// FIXME: cache info so we don't have to set everything up each time we come in here!

int click_x(int sockfd, int kind, xia::XSocketMsg *xsm)
{
   	struct addrinfo hints, *info;
	int rc;
	const char *port, *addr;

	assert(xsm);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if (kind == DATA) {
		port = CLICKDATAPORT;
		addr = CLICKDATAADDRESS;
	} else if (kind == CONTROL) {
		port = CLICKCONTROLPORT;
		addr = CLICKCONTROLADDRESS;
	} else {
		LOG("invalid click port specified");
		return -1;
	}

	// FIXME: gai_strerror is not threadsafe
	if ((rc = getaddrinfo(addr, port, &hints, &info)) < 0) {
		LOGF("getaddrinfo: %s", gai_strerror(rc));
		errno = ECLICKCONTROL;
		return -1;
	}

	std::string p_buf;
	xsm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, info->ai_addr, info->ai_addrlen);

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
	freeaddrinfo(info);

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

	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr(CLICKCONTROLADDRESS);
	sa.sin_port = htons(atoi(CLICKCONTROLPORT));

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

	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr(CLICKCONTROLADDRESS);
	sa.sin_port = htons(atoi(CLICKCONTROLPORT));

	len = sizeof sa;

	memset(buf, 0, buflen);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		LOGF("error(%d) getting reply data from click", errno);
		return -1;
	}

	xia::XSocketMsg reply;
	reply.ParseFromString(buf);
	xia::X_Result_Msg *msg = reply.mutable_x_result();

	// printf(reply.DebugString().c_str());

	*type = msg->type();

	rc = msg->return_code();
	if (rc == -1)
		errno = msg->err_code();

	return rc;
}

int bind_to_random_port(int sockfd)
{
	struct sockaddr_in addr;
	int port;
	int rc;

	srand((unsigned)time(NULL));

	for (int tries = 0; tries < ATTEMPTS; tries++) {
		int rn = rand();
		port=1025 + rn % (65535 - 1024); 

		// printf("trying %d\n", port);
		addr.sin_family = PF_INET;
		addr.sin_addr.s_addr = inet_addr(MYADDRESS);
		addr.sin_port = htons(port);

		rc = bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr));
		if (rc != -1)
			break;
	}

	return rc;
}
