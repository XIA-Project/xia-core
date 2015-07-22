/* ts=4 */
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

#ifndef STATE_H
#define STATE_H

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
	SocketState(int tt, unsigned short port = 0);
	~SocketState();

	int transportType() { return m_transportType; };
	void setTransportType(int tt) {m_transportType = tt; };

	int protocol() { return m_protocol; };
	void setProtocol(int p) {m_protocol = p; };

	int getConnState() { return m_connected; };
	void setConnState(int conn) { m_connected = conn; };

	int isBlocking() { return m_blocking; };
	void setBlocking(int blocking) { m_blocking = blocking; };

	unsigned seqNo();

	int getPacket(unsigned seq, char *buf, unsigned buflen);
	void insertPacket(unsigned seq, char *buf, unsigned buflen);

	void setDebug(int debug) { m_debug = debug; };
	int getDebug() { return m_debug; };

	void setRecvTimeout(struct timeval *timeout) { m_timeout.tv_sec = timeout->tv_sec; m_timeout.tv_usec = timeout->tv_usec; };
	void getRecvTimeout(struct timeval *timeout) {timeout->tv_sec = m_timeout.tv_sec; timeout->tv_usec = m_timeout.tv_usec; };

	const sockaddr_x *peer() { return m_peer; };
	int setPeer(const sockaddr_x *peer);

	int isSIDAssigned() { return m_sid_assigned; };
	void setSIDAssigned() { m_sid_assigned = 1; };

	unsigned short port() { return m_port; };
	void setPort(unsigned short port) { m_port = port; };

	int isTempSID() { return (m_temp_sid == NULL) ? 0 : 1;};
	void setTempSID(const char *sid);
	const char *getTempSID() {return m_temp_sid;};

	void init();
private:
	int m_transportType;
	int m_protocol;
	int m_connected;
	int m_blocking;
	int m_debug;
	sockaddr_x *m_peer;
	char *m_temp_sid;
	int m_sid_assigned;
	unsigned m_sequence;
	unsigned short m_port;
	struct timeval m_timeout;
	pthread_mutex_t m_sequence_lock;
	map<unsigned, string> m_packets;
};

typedef map<int, SocketState *> SMap;


class SocketMap
{
public:
	~SocketMap();

	static SocketMap *getMap();

	void add(int sock, int tt, unsigned short port);
	void remove(int sock);
	SocketState *get(int sock);
	void lock(void) { pthread_rwlock_rdlock(&rwlock); };
	void unlock(void) { pthread_rwlock_unlock(&rwlock); };
	SMap *smap(void) { return &sockets; };

private:
	SocketMap();

	SMap sockets;
	pthread_rwlock_t rwlock;

	static SocketMap *instance;
};

#endif // STATE_H