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

/*!
** @brief Finds the root of the source tree
**
** @returns a character pointer to the root of the source tree
**
*/
char *XrootDir(char *buf, unsigned len) {
	char *dir;
	char *pos;

	if (buf == NULL || len == 0)
		return NULL;

	if ((dir = getenv("XIADIR")) != NULL) {
		strncpy(buf, dir, len);
		return buf;
	}
#ifdef __APPLE__
	if (!getcwd(buf, len)) {
		buf[0] = 0;
		return buf;
	}
#else
	int cnt = readlink("/proc/self/exe", buf, MIN(len, PATH_SIZE));

	if (cnt < 0) {
		buf[0] = 0;
		return buf;
	}
	else if ((unsigned)cnt == len)
		buf[len - 1] = 0;
	else
		buf[cnt] = 0;
#endif
	pos = strstr(buf, SOURCE_DIR);
	if (pos) {
		pos += sizeof(SOURCE_DIR) - 1;
		*pos = '\0';
	}
	return buf;
}

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
	static int initialized = 0;
	static struct sockaddr_in sa;

	assert(xsm);

	// FIXME: have I created a race condition here?
	if (!initialized) {

		sa.sin_family = PF_INET;
		sa.sin_addr.s_addr = inet_addr("127.0.0.1");
		sa.sin_port = htons(atoi(CLICKPORT));

		initialized = 1;
	}

	std::string p_buf;
	xsm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();
	while (remaining > 0) {
		setWrapped(sockfd, TRUE);
		rc = sendto(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa));
		setWrapped(sockfd, FALSE);

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

int click_get(int sock, unsigned seq, char *buf, unsigned buflen, xia::XSocketMsg *msg)
{
	int rc;

	// FIXME: if someone caches our packet as we start to do the recv, we'll block forever
	// may need to implement select so that we re-loop frequently so we can pick up our
	// cached packet

	while (1) {
		// see if another thread received and cached our packet
		if ((rc = getCachedPacket(sock, seq, buf, buflen)) > 0) {
			LOGF("Got cached response with sequence # %d\n", seq);
			std::string s(buf, rc);
			msg->ParseFromString(s);
			break;

		} else {
			setWrapped(sock, TRUE);
			rc = recvfrom(sock, buf, buflen - 1 , 0, NULL, NULL);
			setWrapped(sock, FALSE);

			if (rc < 0) {
				LOGF("error(%d) getting reply data from click", errno);
				rc = -1;
				break;

			} else {
				std::string s(buf, rc);
				msg->ParseFromString(s);
				assert(msg);
				unsigned sn = msg->sequence();

				if (sn == seq)
					break;

				// these are not the data you were looking for
				LOGF("Expected packet %u, received %u, caching packet\n", seq, sn);
				cachePacket(sock, sn, buf, buflen);
				msg->Clear();
			}
		}
	}

	return rc;
}

int click_reply(int sock, unsigned seq, xia::XSocketMsg *msg)
{
	char buf[XIA_MAXBUF];
	unsigned buflen = sizeof(buf);

	return click_get(sock, seq, buf, buflen, msg);
}

int click_status(int sock, unsigned seq)
{
	char buf[XIA_MAXBUF];
	unsigned buflen = sizeof(buf);
	int rc;
	xia::XSocketMsg msg;

	if ((rc = click_get(sock, seq, buf, buflen, &msg)) >= 0) {

		xia::X_Result_Msg *res = msg.mutable_x_result();

		rc = res->return_code();
		if (rc == -1)
			errno = res->err_code();
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

// FIXME: implement log handlers that use SO_DEBUG value to decide whether or not to log
// FIXME: implement handler to set output for log messages, file* or syslog

