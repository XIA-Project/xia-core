/* ts=4 */
/*
** Copyright 2012 Carnegie Mellon University
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
  @file state.c
  @brief implements internal socket state functionality.
*/
#include <map>
#include <string>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

using namespace std;

class SocketState
{
public:
	SocketState();
	SocketState(int tt);
	~SocketState();

	int transportType() { return m_transportType; };
	void setTransportType(int tt) {m_transportType = tt; };

	int data(char *buf, unsigned bufLen);
	void setData(const char *buf, unsigned bufLen);
	int dataLen() { return m_bufLen; };

	int getConnState() { return m_connected; };
	void setConnState(int conn) { m_connected = conn; };

	int isWrapped() { return (m_wrapped != 0); };
	void setWrapped(int wrap) { wrap ? m_wrapped++ : m_wrapped--; };

	int isAsync() { return m_async; };
	void setAsync(int async) { m_async = async; };

	unsigned seqNo() { return m_sequence++; };

	int getPacket(unsigned seq, char *buf, unsigned buflen);
	void insertPacket(unsigned seq, char *buf, unsigned buflen);

	void setDebug(int debug) { m_debug = debug; };
	int getDebug() { return m_debug; };

	void setError(int error) { m_error = error; };
	int getError() { return m_error; };

	const sockaddr_x *peer() { return m_peer; };
	int setPeer(const sockaddr_x *peer);

private:
	int m_transportType;
	int m_connected;
	int m_async;
	int m_wrapped;	// hack for dealing with xwrap stuff
	int m_debug;
	int m_error;
	char *m_buf;
	sockaddr_x *m_peer;
	unsigned m_bufLen;
	unsigned m_sequence;
	map<unsigned, string> m_packets;
};

SocketState::SocketState(int tt)
{
	::SocketState();
	m_transportType = tt;
	m_connected = 0;
	m_async = 0;
	m_wrapped = 0;
	m_buf = NULL;
	m_peer = NULL;
	m_bufLen = 0;
	m_sequence = 1;
	m_debug = 0;
	m_error = 0;
}

SocketState::SocketState()
{
	m_transportType = -1;
	m_connected = 0;
	m_async = 0;
	m_wrapped = 0;
	m_buf = NULL;
	m_peer = NULL;
	m_bufLen = 0;
	m_sequence = 1;
	m_debug = 0;
	m_error = 0;
}

SocketState::~SocketState()
{
	if (m_buf)
		delete(m_buf);
}

int SocketState::data(char *buf, unsigned bufLen)
{
	if (m_bufLen == 0) {
		// we don't have anything stashed away
		return 0;

	} else if (m_bufLen > bufLen) {
		// give the caller as much as we can, and hang on to the rest
		// for later
		memcpy(buf, m_buf, bufLen);
		m_bufLen -= bufLen;
		memmove(m_buf, m_buf + bufLen, m_bufLen);

	} else {
		// get rid of the data and reset our state
		bufLen = m_bufLen;
		memcpy(buf, m_buf, m_bufLen);
		delete(m_buf);
		m_buf = (char *)0;
		m_bufLen = 0;
	}
	return bufLen;
}

void SocketState::setData(const char *buf, unsigned bufLen)
{
	if (!buf || bufLen == 0)
		return;

	assert(!m_buf && m_bufLen == 0);

	m_buf = new char [bufLen];
	if (!m_buf)
		return;

	memcpy(m_buf, buf, bufLen);
	m_bufLen = bufLen;
}

int SocketState::getPacket(unsigned seq, char *buf, unsigned buflen)
{
	int rc = 0;
	map<unsigned, string>::iterator it;

	it = m_packets.find(seq);
	if (it != m_packets.end()) {
		string s = it->second;

		rc = MIN(buflen, s.size());
		memcpy(buf, s.c_str(), rc);

		m_packets.erase(seq);
	}

	return rc;
}

void SocketState::insertPacket(unsigned seq, char *buf, unsigned buflen)
{
	std::string s(buf, buflen);
	m_packets[seq] = s;
}

class SocketMap
{
public:
	~SocketMap();

	static SocketMap *getMap();

	void add(int sock, int tt);
	void remove(int sock);
	SocketState *get(int sock);

private:
	SocketMap();

	map<int, SocketState *> sockets;
	pthread_rwlock_t rwlock;

	static SocketMap *instance;
};

SocketMap::SocketMap()
{
	pthread_rwlock_init(&rwlock, NULL);
}

SocketMap::~SocketMap()
{
	pthread_rwlock_destroy(&rwlock);
}

SocketMap *SocketMap::instance = (SocketMap *)0;

SocketMap *SocketMap::getMap()
{
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	if (!instance) {

		pthread_mutex_lock(&lock);

		if (!instance)
			instance = new SocketMap();

		pthread_mutex_unlock(&lock);
	}
	return instance;
}

void SocketMap::add(int sock, int tt)
{
	SocketMap *state = getMap();
	pthread_rwlock_wrlock(&rwlock);
	if (state->sockets[sock] == 0)
		state->sockets[sock] = new SocketState(tt);
	pthread_rwlock_unlock(&rwlock);
}

void SocketMap::remove(int sock)
{
	SocketMap *state = getMap();

	pthread_rwlock_wrlock(&rwlock);
	state->sockets.erase(sock);
	pthread_rwlock_unlock(&rwlock);
}

SocketState *SocketMap::get(int sock)
{
	// because a socket will only be acted on by one thread at a time,
	// we don't need to be more protective of the stat structure

	pthread_rwlock_rdlock(&rwlock);
	SocketState *p =  SocketMap::getMap()->sockets[sock];
	pthread_rwlock_unlock(&rwlock);
	return p;
}

int SocketState::setPeer(const sockaddr_x *peer)
{
	if (m_peer != NULL)
		free(m_peer);

	m_peer = (sockaddr_x *)malloc(sizeof(sockaddr_x));
	memcpy(m_peer, peer, sizeof(sockaddr_x));
	return 0;
}

// extern "C" {

void allocSocketState(int sock, int tt)
{
	SocketMap::getMap()->add(sock, tt);
}

void freeSocketState(int sock)
{
	SocketMap::getMap()->remove(sock);
}

int getSocketType(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->transportType();
	else
		return -1;
}

int getConnState(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->getConnState();
	else
		return UNKNOWN_STATE;
}

void setConnState(int sock, int conn)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setConnState(conn);
}

int isWrapped(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->isWrapped();
	else
		return 0;
}

void setWrapped(int sock, int wrapped)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setWrapped(wrapped);
}

int isAsync(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->isAsync();
	else
		return 0;
}

void setAsync(int sock, int async)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setAsync(async);
}

int getDebug(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->getDebug();
	else
		return 0;
}

void setDebug(int sock, int debug)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setDebug(debug);
}

int getError(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->getError();
	else
		return 0;
}

void setError(int sock, int error)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setError(error);
}

void setSocketType(int sock, int tt)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setTransportType(tt);
}

int getSocketData(int sock, char *buf, unsigned bufLen)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->data(buf, bufLen);
	else
		return 0;
}

void setSocketData(int sock, const char *buf, unsigned bufLen)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		sstate->setData(buf, bufLen);
}

unsigned seqNo(int sock)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate)
		return sstate->seqNo();
	else
		return 0;
}

void cachePacket(int sock, unsigned seq, char *buf, unsigned buflen)
{
	SocketState *sstate = SocketMap::getMap()->get(sock);
	if (sstate) {
		sstate->insertPacket(seq, buf, buflen);
	}
}

int getCachedPacket(int sock, unsigned seq, char *buf, unsigned buflen)
{
	int rc = 0;
	SocketState *sstate = SocketMap::getMap()->get(sock);

	if (sstate) {
		rc = sstate->getPacket(seq, buf, buflen);
	}

	return rc;
}

int connectDgram(int sock, sockaddr_x *addr)
{
	int rc = 0;
	SocketState *sstate = SocketMap::getMap()->get(sock);

	if (sstate) {
		sstate->setConnState((addr == NULL) ? UNCONNECTED : CONNECTED);
		sstate->setPeer(addr);
	}

	return rc;
}

#if 0
int main()
{
	char buf[1024];
	int len;

	// should be invalid
	printf("socket %d tt %d\n", 0, getSocketType(0));

	// should be valid, then invalid
	allocSocketState(5, 1);
	printf("socket %d tt %d\n", 5, getSocketType(5));
	freeSocketState(5);

	allocSocketState(2, 3);
	printf("socket %d tt %d conn %d\n", 2, getSocketType(2), isConnected(2));
	setConnected(2, 1);
	printf("socket %d tt %d conn %d\n", 2, getSocketType(2), isConnected(2));


	allocSocketState(55, 2);
	printf("socket %d tt %d\n", 55, getSocketType(55));
	setSocketType(55, 3);
	printf("socket %d tt %d\n", 55, getSocketType(55));

	len = getSocketData(55, buf, 1024);
	printf("socket %d buflen %d\n", 55, len);

	const char *p = "0123456789";
	setSocketData(55, p, 10);
	len = getSocketData(55, buf, 5);
	buf[5] = 0;
	printf("sock %d len %d buf %s\n", 55, len, buf);
	len = getSocketData(55, buf, 1024);
	buf[len] = 0;
	printf("sock %d len %d buf %s\n", 55, len, buf);

	setSocketData(2, p, 10);
	len = getSocketData(2, buf, 1024);
	buf[len] = 0;
	printf("sock %d len %d buf %s\n", 2, len, buf);
	len = getSocketData(2, buf, 1024);
	buf[len] = 0;
	printf("sock %d len %d buf %s\n", 2, len, buf);

	len = getSocketData(100, buf, 1024);
	printf("sock %d len %d\n", 100, len);
}
#endif

//}

