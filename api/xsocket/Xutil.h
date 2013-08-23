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
  @file Xutil.h
  @brief header for internal helper functions
*/
#ifndef _Xutil_h
#define _Xutil_h


#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif

#define WOULDBLOCK()	(errno == EAGAIN || errno == EWOULDBLOCK)

#define UNCONNECTED	0
#define CONNECTING	1
#define CONNECTED 	2

int click_send(int sockfd, xia::XSocketMsg *xsm);
int click_reply(int sockfd, xia::XSocketCallType type, char *buf, int buflen);
int click_reply2(int sockfd, xia::XSocketCallType *type);
int click_reply3(int sockfd, xia::XSocketCallType type, xia::XSocketMsg &msg);
int bind_to_random_port(int sockfd);

int validateSocket(int sock, int stype, int err);

// socket state functions for internal API use
// implementation is in state.c
void allocSocketState(int sock, int tt);
void freeSocketState(int sock);
int getSocketType(int sock);
void setSocketType(int sock, int tt);
int connState(int sock);
void setConnState(int sock, int conn);
int getSocketData(int sock, char *buf, unsigned bufLen);
void setSocketData(int sock, const char *buf, unsigned bufLen);
void setWrapped(int sock, int wrapped);
int isWrapped(int sock);
void setBlocking(int sock, int blocking);
int isBlocking(int sock);
void setError(int sock, int error);
int getError(int sock);
void addPacket(int sock, xia::XSocketMsg &msg);
int getPacket(int sock, xia::XSocketMsg &msg, xia::XSocketCallType mtype);
int setPeer(int sock, sockaddr_x *addr);
const sockaddr_x *dgramPeer(int sock);

int _xsendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr_x *addr, socklen_t addrlen);
int _xrecvfromconn(int sockfd, void *buf, size_t len, int flags);

#endif
