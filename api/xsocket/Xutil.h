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

#define PATH_SIZE 4096

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define UNKNOWN_STATE 0
#define UNCONNECTED	  1
#define CONNECTING	  2
#define CONNECTED 	  3

#define WOULDBLOCK()	`(errno == EAGAIN || errno == EWOULDBLOCK)

int click_send(int sockfd, xia::XSocketMsg *xsm);
int click_reply(int sockfd, unsigned seq, xia::XSocketMsg *msg);
int click_status(int sockfd, unsigned seq);

int validateSocket(int sock, int stype, int err);

// socket state functions for internal API use
// implementation is in state.c
void allocSocketState(int sock, int tt);
void freeSocketState(int sock);
int getSocketType(int sock);
void setSocketType(int sock, int tt);
int getConnState(int sock);
void setConnState(int sock, int conn);
int getSocketData(int sock, char *buf, unsigned bufLen);
void setSocketData(int sock, const char *buf, unsigned bufLen);
void setWrapped(int sock, int wrapped);
int isWrapped(int sock);
int isAPI(int sock);
void setAPI(int sock, int api);
void setBlocking(int sock, int blocking);
int isBlocking(int sock);
void setDebug(int sock, int debug);
int getDebug(int sock);
void setRecvTimeout(int sock, struct timeval *timeout);
void getRecvTimeout(int sock, struct timeval *timeout);
void setError(int sock, int error);
int getError(int sock);
unsigned seqNo(int sock);
void cachePacket(int sock, unsigned seq, char *buf, unsigned buflen);
int getCachedPacket(int sock, unsigned seq, char *buf, unsigned buflen);
int connectDgram(int sock, sockaddr_x *addr);
const sockaddr_x *dgramPeer(int sock);

int _xsendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr_x *addr, socklen_t addrlen);
int _xrecvfromconn(int sockfd, void *buf, size_t len, int flags);

#endif
