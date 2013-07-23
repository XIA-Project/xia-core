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

#define XID_CHARS (XID_SIZE * 2)


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

int click_send(int sockfd, xia::XSocketMsg *xsm)
{
	int rc = 0;
	struct sockaddr_in sa;

	assert(xsm);

	// TODO: cache these so we don't have to set everything up each time we
	// are called
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
    sa.sin_port = htons(atoi(CLICKPORT));

	std::string p_buf;
	xsm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa));

		if (rc == -1) {
			if (!WOULDBLOCK()) {
				LOGF("click socket failure: errno = %d", errno);
			}
			break;
		} else {
			remaining -= rc;
			p += rc;
			if (remaining > 0) {
				LOGF("%d bytes left to send", remaining);

				// FIXME: click will crash if we need to send more than a
				// single buffer to get the entire block of data sent.
				LOG("click can't handle partial packets");
				rc = -1;
				break;
			}
		}
	}

	return  (rc >= 0 ? 0 : -1);
}

int click_reply(int sockfd, xia::XSocketCallType type, char *buf, int buflen)
{
	struct sockaddr_in sa;
	socklen_t len;
	int rc;

	len = sizeof sa;

	// check to see if we have a stored packet

	memset(buf, 0, buflen);
	if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, (struct sockaddr *)&sa, &len)) < 0) {
		if (!WOULDBLOCK()) {
			LOGF("error(%d) getting reply data from click", errno);
		}
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
		if (!WOULDBLOCK()) {
			LOGF("error(%d) getting reply data from click", errno);
		}
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

int click_reply3(int sockfd, xia::XSocketCallType type, xia::XSocketMsg &msg)
{
	char buf[MAXBUFLEN];
	int buflen = sizeof(buf);
	int rc;

	// FIXME: check to see if we have a stored packet

	for (;;) {

		if ((rc = recvfrom(sockfd, buf, buflen - 1 , 0, NULL, NULL)) < 0) {
			if (errno != EAGAIN || errno != EWOULDBLOCK) {
				LOGF("error(%d) getting reply data from click", errno);
			}
			break;
		}

		// FIXME: connect should really use protobufs!
		if (!isBlocking(sockfd)) {
			if (strcmp(buf, "Connection_granted") == 0) {
				// we got a connect reply, mark socket as connected
				setConnState(sockfd, CONNECTED);
				setError(sockfd, 0);
				// now go back and loop looking for data
			} else if (strcmp(buf, "^Connection-failed^") == 0) {
				setConnState(sockfd, UNCONNECTED);
				setError(sockfd, ECONNREFUSED);
				errno = ECONNREFUSED;
				break;
			}
		}

		std::string str(buf, buflen);
		xia::XSocketMsg xsm;
		xsm.ParseFromString(str);

		// is it what we were expecting?
		if (xsm.type() == type) {
			msg = xsm;
			rc = 0;
			break;

		} else if (xsm.type() == xia::XCONNECT) {
			setConnState(sockfd, CONNECTED);

		} else {
			LOG("unexpected packet, queueing and looping...");
		}
	}

	return rc;
}

int checkXid(const char *xid, const char *type)
{
	const char *p;
	const char *colon = NULL;
	int rc = 0;

	if (type && strlen(type) > 0) {
		if (strncmp(xid, type, strlen(type)) != 0)
			return 0;
	}

	for (p = xid; *p; p++) {
		if (*p == ':') {

			if (colon) {
				// FAIL, we already found one
				break;
			}
			if (p == xid) {
				// FAIL, colon is first character
				break;
			}
			colon = p;

		} else if (colon) {
			if (!isxdigit(*p)) {
				// FAIL, the hash string is invalid
				break;
			}

		} else if (!isalnum(*p)) {
			// FAIL, the XID type is invalid
			break;
		}
	}

	if (colon && (p - colon - 1 == XID_CHARS))
		rc = 1;

	return rc;
}

// currently this expects the DAG to be in RE ... format
// may need to make it smarter later
int checkDag(const char *dag)
{
	int valid = 1;
	int fallback = 0;
	int inXid = 0;
	char *xid = NULL;

	if (strncmp(dag, "RE ", 3) != 0)
		return 0;

	// make a copy so we can step on it
	char *buf = strdup(dag);
	char *p = buf + 3;

	while (*p != '\0') {

		if (*p == ' ') {
			if (inXid) {
				*p = 0;
				if (!checkXid(xid, NULL)) {
					valid = 0;
					break;
				}
				inXid = 0;
			}

		} else if (*p == '(') {
			// parens need a space or nul on either side
			if ((*(p-1) != ' ' && *(p-1) != '\0') || (*(p+1) != ' ' && *(p+1) != '\0')) {
				valid = 0;
				break;
			}
			fallback++;

		} else if (*p == ')') {
			// parens need a space on either side
			if ((*(p-1) != ' ' && *(p-1) != '\0') || (*(p+1) != ' ' && *(p+1) != '\0')) {
				valid = 0;
				break;
			}
			fallback--;

		} else if (!inXid) {
			inXid = 1;
			xid = p;
		}

		p++;
	}

	if (fallback != 0) {
		valid = 0;

	} else  if (inXid) {
		valid = checkXid(xid, NULL);
	}

	free(buf);
	return valid;
}
