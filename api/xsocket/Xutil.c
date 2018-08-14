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
#include <strings.h>

// for printfing param values
#include <fcntl.h>
#include <netdb.h>

#define CONTROL 1
#define DATA 2

#define XID_CHARS (XID_SIZE * 2)

// FIXME: do this smarter
// this should be larger than the size of the maximum amount XIA can transfer, but smaller than localhost MTU
// optimally, XIA_MAXBUF would be calculated based off of this and the maximum size of a protobuf message
#define XIA_INTERNAL_BUFSIZE	(16 * 1024)


// dump contents of various parameters passed to functions
#define FR(f) { f, #f }

idrec xfer_flags[] = {
	FR(MSG_CMSG_CLOEXEC),
	FR(MSG_CONFIRM),
	FR(MSG_CTRUNC),
	FR(MSG_DONTROUTE),
	FR(MSG_DONTROUTE),
	FR(MSG_DONTWAIT),
	FR(MSG_EOR),
	FR(MSG_ERRQUEUE),
	FR(MSG_FASTOPEN),
	FR(MSG_FIN),
	FR(MSG_MORE),
	FR(MSG_NOSIGNAL),
	FR(MSG_OOB),
	FR(MSG_PEEK),
	FR(MSG_PROXY),
	FR(MSG_RST),
	FR(MSG_SYN),
	FR(MSG_TRUNC),
	FR(MSG_WAITALL),
	FR(MSG_WAITFORONE)
};

idrec ai_flags[] = {
	FR(AI_ADDRCONFIG),
	FR(AI_ALL),
	FR(AI_CANONIDN),
	FR(AI_CANONNAME),
	FR(AI_IDN),
	FR(AI_IDN_ALLOW_UNASSIGNED),
	FR(AI_IDN_USE_STD3_ASCII_RULES),
	FR(AI_NUMERICHOST),
	FR(AI_NUMERICSERV),
	FR(AI_PASSIVE),
	FR(AI_V4MAPPED)
};

idrec proto_values[] = {
	FR(IPPROTO_IP),
	FR(IPPROTO_ICMP),
	FR(IPPROTO_IGMP),
	FR(IPPROTO_IPIP),
	FR(IPPROTO_TCP),
	FR(IPPROTO_EGP),
	FR(IPPROTO_PUP),
	FR(IPPROTO_UDP),
	FR(IPPROTO_IDP),
	FR(IPPROTO_TP),
	FR(IPPROTO_DCCP),
	FR(IPPROTO_IPV6),
	FR(IPPROTO_RSVP),
	FR(IPPROTO_GRE),
	FR(IPPROTO_ESP),
	FR(IPPROTO_AH),
	FR(IPPROTO_MTP),
	FR(IPPROTO_BEETPH),
	FR(IPPROTO_ENCAP),
	FR(IPPROTO_PIM),
	FR(IPPROTO_COMP),
	FR(IPPROTO_SCTP),
	FR(IPPROTO_UDPLITE),
	FR(IPPROTO_RAW)
};

idrec fcntl_flags[] = {
	FR(O_APPEND),
	FR(O_ASYNC),
	FR(O_CLOEXEC),
	FR(O_DIRECT),
	FR(O_NOATIME),
	FR(O_NONBLOCK),
	FR(O_RDWR),
	FR(O_WRONLY)
};

idrec fcntl_cmds[] = {
	FR(F_DUPFD),
	FR(F_DUPFD_CLOEXEC),
	FR(F_SETFD),
	FR(F_SETFL),
	FR(F_SETOWN),
	FR(F_SETOWN_EX),
	FR(F_SETSIG),
	FR(F_SETLEASE),
	FR(F_NOTIFY),
	FR(F_SETPIPE_SZ),
	FR(F_GETLK),
	FR(F_SETLK),
	FR(F_SETLKW),
	FR(F_GETFD),
	FR(F_GETFL),
	FR(F_GETOWN),
	FR(F_GETOWN_EX),
	FR(F_GETSIG),
	FR(F_GETLEASE),
	FR(F_GETPIPE_SZ)
};

idrec poll_flags[] = {
	FR(POLLERR),
	FR(POLLHUP),
	FR(POLLIN),
	FR(POLLMSG),
	FR(POLLNVAL),
	FR(POLLOUT),
	FR(POLLPRI),
	FR(POLLRDBAND),
	FR(POLLRDHUP),
	FR(POLLRDNORM),
	FR(POLLREMOVE),
	FR(POLLWRBAND),
	FR(POLLWRNORM)
};

idrec af_values[] = {
	FR(AF_FILE),
	FR(AF_INET),
	FR(AF_INET6),
	FR(AF_LOCAL),
	FR(AF_PACKET),
	FR(AF_UNIX),
	FR(AF_UNSPEC),
	FR(AF_XIA)
};

idrec opt_values[] = {
	FR(SO_BROADCAST),
	FR(SO_BSDCOMPAT),
	FR(SO_DEBUG),
	FR(SO_DONTROUTE),
	FR(SO_ERROR),
	FR(SO_KEEPALIVE),
	FR(SO_LINGER),
	FR(SO_NO_CHECK),
	FR(SO_OOBINLINE),
	FR(SO_PASSCRED),
	FR(SO_PEERCRED),
	FR(SO_PRIORITY),
	FR(SO_RCVBUF),
	FR(SO_RCVBUFFORCE),
	FR(SO_RCVLOWAT),
	FR(SO_RCVTIMEO),
	FR(SO_REUSEADDR),
	FR(SO_REUSEPORT),
	FR(SO_SNDBUF),
	FR(SO_SNDBUFFORCE),
	FR(SO_SNDLOWAT),
	FR(SO_SNDTIMEO),
	FR(SO_TYPE)
};


/*!
* @brief Prints some debug information
*
* @param sock
*/
void debug(int sock) {
	printf("[ sock %d/%d, thread %p ]\t", sock, getPort(sock), (void*)pthread_self());
}

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
	if (!getcwd(buf, len)) {
		buf[0] = 0;
		return buf;
	}
	pos = strstr(buf, SOURCE_DIR);
	if (pos) {
		pos += sizeof(SOURCE_DIR) - 1;
		*pos = '\0';
	}
	return buf;
}

const char *XnetworkDAGIntentAD(const char *network_dag)
{
    // The last entry in the network_dag must be an AD
    const char *last_ad = rindex(network_dag, ':') - 2;
    if(strncmp("AD:", last_ad, 3) == 0) {
        return last_ad;
    }
    return NULL;
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

	if (isBlocking(sockfd)) {
		// make sure click know if it should reply immediately or not
		xsm->set_blocking(true);
	}

	xsm->set_port(getPort(sockfd));
	xsm->set_id(getID(sockfd));

	std::string p_buf;
	xsm->SerializeToString(&p_buf);

	int remaining = p_buf.size();
	const char *p = p_buf.c_str();

	while (remaining > 0) {

		//LOGF("sending to click: seq: %d type: %d", xsm->sequence(), xsm->type());
		rc = (_f_sendto)(sockfd, p, remaining, 0, (struct sockaddr *)&sa, sizeof(sa));

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

	if (isBlocking(sock)) {
		// make sure click knows if it should reply immediately or not
		msg->set_blocking(true);
	}

	while (1) {
		// see if another thread received and cached our packet
		if (0) {
//		if ((rc = getCachedPacket(sock, seq, buf, buflen)) > 0) {

			LOGF("Got cached response with sequence # %d\n", seq);
			std::string s(buf, rc);
			msg->ParseFromString(s);
			break;

		} else {

			bzero(buf, buflen);
			// we do this with a blocking socket even if the Xsocket is marked as nonblocking.
			// The UDP socket is treated as an API call rather than a sock so making it
			// non-blocking would cause problems
			rc = (_f_recvfrom)(sock, buf, buflen - 1 , 0, NULL, NULL);

//			LOGF("seq %d received %d bytes\n", seq, rc);

			if (rc < 0) {
				// try to guard against the weird errors we see on GENI
				if (errno == ENOBUFS || errno == EIO) {
					printf("got annoying error, looping: %s\n", strerror(errno));
					sleep(1);
					continue;
				}
				if (isBlocking(sock) || (errno != EWOULDBLOCK && errno != EAGAIN)) {
					LOGF("error(%d) getting reply data from click", errno);
				}
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
				LOGF("Expected packet %u, received %u, replaying packet\n", seq, sn);
//				cachePacket(sock, sn, buf, buflen);
//				msg->PrintDebugString();

				// shove this back into click so the requester can get at it
				xia::X_Replay_Msg *xrm = msg->mutable_x_replay();
				xrm->set_sequence(msg->sequence());
				xrm->set_type(msg->type());

				msg->set_sequence(0);
				msg->set_type(xia::XREPLAY);

				click_send(sock, msg);

				msg->Clear();
			}
		}
	}

	return rc;
}

int click_reply(int sock, unsigned seq, xia::XSocketMsg *msg)
{
	int rc;
	int e = 0;
	unsigned buflen = api_mtu();
	char *buf = (char *)malloc(buflen);

	if ((rc = click_get(sock, seq, buf, buflen, msg)) >= 0) {

		xia::X_Result_Msg *res = msg->mutable_x_result();

		rc = res->return_code();
		if (rc == -1)
			e = res->err_code();
	}

	free(buf);
	errno = e;

	return rc;
}

int click_status(int sock, unsigned seq)
{
	unsigned buflen = api_mtu();
	char *buf = (char *)malloc(buflen);
	int e = 0;
	int rc;
	xia::XSocketMsg msg;

	if ((rc = click_get(sock, seq, buf, buflen, &msg)) >= 0) {

		xia::X_Result_Msg *res = msg.mutable_x_result();

		rc = res->return_code();
		if (rc == -1)
			e = res->err_code();
	}

	free(buf);
	errno = e;
	return rc;
}

int MakeApiSocket(int transport_type)
{
	struct sockaddr_in sa;
	int sock;
	unsigned short port;

	socklen_t len = sizeof(sa);

	if ((sock = (_f_socket)(AF_INET, SOCK_DGRAM, 0)) == -1) {
		LOGF("Error creating socket: %s", strerror(errno));
		return sock;
	}

	// bind to an unused random port number
	sa.sin_family = PF_INET;
	sa.sin_addr.s_addr = inet_addr("127.0.0.1");
	sa.sin_port = 0;

	if ((_f_bind)(sock, (const struct sockaddr *)&sa, sizeof(sa)) < 0) {
		(_f_close)(sock);

		LOGF("Error binding new socket to local port: %s", strerror(errno));
		return -1;
	}

	if((_f_getsockname)(sock, (struct sockaddr *)&sa, &len) < 0) {
		LOGF("Error retrieving socket's UDP port: %s", strerror(errno));
		port = 0;
	} else {
		port = ((struct sockaddr_in)sa).sin_port;
	}

	allocSocketState(sock, transport_type, port);

	return sock;
}

// print out the list of flags in the bitmap
// called via the FOO_FLAGS macro
static const char *getFlags(idrec *table, size_t size, size_t f)
{
	static char s[1024];
	size_t mask = 0;

	if (f == 0){
		return "0";
	}

	s[0] = 0;
	int found = 0;
	for (size_t i = 0; i < size; i++) {
		mask |= table[i].id;

		if (f & table[i].id) {
			if (found) {
				strcat(s, " ");
			}
			strcat(s, table[i].name);
			found = 1;
		}
	}

	size_t remaining = f & ~mask;
	if (remaining) {
		size_t len = strlen(s);
		sprintf(s+len, "UNKNOWN(0x%08x)", (int)remaining);
	}

	return s;
}

// calculate the number of bytes in the iovec
size_t _iovSize(const struct iovec *iov, size_t iovcnt)
{
	size_t size = 0;

	for (size_t i = 0; i < iovcnt; i++) {
		size += iov[i].iov_len;
	}

	return size;
}



// Flatten an iovec into a single buffer
size_t _iovPack(const struct iovec *iov, size_t iovcnt, char **buf)
{
	size_t size = _iovSize(iov, iovcnt);
	char *p;

	p = *buf = (char *)malloc(size);

	for (size_t i = 0; i < iovcnt; i++) {
		memcpy(p, iov[i].iov_base, iov[i].iov_len);
		p += iov[i].iov_len;
	}

	return size;
}



// unload a buffer into an iovec
int _iovUnpack(const struct iovec *iov, size_t iovcnt, char *buf, size_t len)
{
	int rc = 0;
	size_t size = 0;
	char *p = buf;

	size = _iovSize(iov, iovcnt);

	if (size < len) {
		// there's more data than we have room for
		rc = -1;
	}

	for (size_t i = 0; i < iovcnt; i++) {
		if (size == 0 || len == 0)
			break;

		int cnt = MIN(size, iov[i].iov_len);

		memcpy(iov[i].iov_base, p, cnt);
		p += cnt;
		size -= cnt;
		len -= cnt;
	}

	return rc;
}

// print out name of the parameter
// called via the FOO_VALUE macro
static const char *getValue(idrec *table, size_t size, size_t v)
{
	static char s[64];

	if (v == 0) {
		return "0";
	}

	for (size_t i = 0; i < size; i++) {

		if (v == table[i].id) {
			return table[i].name;
		}
	}

	sprintf(s, "UNKNOWN(%zu)", v);
	return s;
}

const char *xferFlags(size_t f)
{
	return getFlags(xfer_flags, sizeof(xfer_flags)/sizeof(idrec), f);
}

const char *aiFlags(size_t f)
{
	return getFlags(ai_flags, sizeof(ai_flags)/sizeof(idrec), f);
}

const char *fcntlFlags(size_t f)
{
	return getFlags(fcntl_flags, sizeof(fcntl_flags)/sizeof(idrec), f);
}

const char *pollFlags(size_t f)
{
	return getFlags(poll_flags, sizeof(poll_flags)/sizeof(idrec), f);
}

const char *afValue(size_t f)
{
	return getValue(af_values, sizeof(af_values)/sizeof(idrec), f);
}

const char *optValue(size_t f)
{
	return getValue(opt_values, sizeof(opt_values)/sizeof(idrec), f);
}

const char *protoValue(size_t f)
{
	return getValue(proto_values, sizeof(proto_values)/sizeof(idrec), f);
}

const char *fcntlCmd(int c)
{
	return getValue(fcntl_cmds, sizeof(fcntl_cmds)/sizeof(idrec), c);
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
