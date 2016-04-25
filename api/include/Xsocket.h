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
  @file Xsocket.h
  @brief Xsocket API header file
*/

#ifndef XSOCKET_H
#define XSOCKET_H


#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>

#include "xia.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXBUFLEN    65535 // Note that this limits the size of chunk we can receive TODO: What should this be?
#define XIA_MAXBUF   MAXBUFLEN
#define XIA_MAXCHUNK MAXBUFLEN

// for python swig compiles
#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif
#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif

#define XSOCK_INVALID -1			// invalid socket type
#define XSOCK_STREAM SOCK_STREAM	// Reliable transport (SID)
#define XSOCK_DGRAM  SOCK_DGRAM		// Unreliable transport (SID)
#define XSOCK_RAW	 SOCK_RAW		// Raw XIA socket
#define XSOCK_CHUNK  4				// Content Chunk transport (CID)

#define REQUEST_FAILED    0x00000001
#define WAITING_FOR_CHUNK 0x00000002
#define READY_TO_READ     0x00000004
#define INVALID_HASH      0x00000008

// Cache policy
#define POLICY_LRU				0x00000001
#define POLICY_FIFO				0x00000002
#define POLICY_REMOVE_ON_EXIT	0x00001000
#define POLICY_RETAIN_ON_EXIT	0x00002000
#define POLICY_DEFAULT			(POLICY_LRU | POLICY_RETAIN_ON_EXIT)

#define CID_HASH_SIZE 40

#define DEFAULT_CHUNK_SIZE	2000


// XIA specific addrinfo flags
#define XAI_DAGHOST	AI_NUMERICHOST	// if set, name is a dag instead of a generic name string
#define XAI_XIDSERV	AI_NUMERICSERV	// if set, service is an XID instead of a name string
#define XAI_FALLBACK 0x10000 		// if set, wrap the dag in parens


// XIA specific getaddrinfo error codes (move to xia.h)
#define XEAI_UNIMPLEMENTED	-8000

// Xsetsockopt options
#define XOPT_HLIM		0x07001	// Hop Limit TTL
#define XOPT_NEXT_PROTO	0x07002	// change the next proto field of the XIA header
#define XOPT_BLOCK		0x07003
#define XOPT_ERROR_PEEK 0x07004

// XIA protocol types
#define XPROTO_XIA_TRANSPORT	0x0e
#define XPROTO_XCMP		0x3d

// error codes
#define ECLICKCONTROL 9999	// error code for general click communication errors

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

//Function list
extern int Xsocket(int family, int transport_type, int protocol);
extern int Xaccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int XacceptAs(int sockfd, struct sockaddr *remote_addr, socklen_t *remote_addrlen, struct sockaddr *addr, socklen_t *addrlen);
extern int Xaccept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
extern int Xbind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int Xconnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int Xpoll(struct pollfd *ufds, unsigned nfds, int timeout);
extern int Xlisten(int sockfd, int backlog);
extern int Xrecvfrom(int sockfd,void *rbuf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen);
extern ssize_t Xrecvmsg(int fd, struct msghdr *msg, int flags);
extern int Xsendto(int sockfd,const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen);
extern ssize_t Xsendmsg(int fd, const struct msghdr *msg, int flags);


extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *rbuf, size_t len, int flags);
extern int Xsend(int sockfd, const void *buf, size_t len, int flags);
extern int Xfcntl(int sockfd, int cmd, ...);
extern int Xselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);
extern int Xfork(void);
extern int Xnotify(void);

#if 0
extern int XrequestChunk(int sockfd, char* dag, size_t dagLen);
extern int XrequestChunks(int sockfd, const ChunkStatus *chunks, int numChunks);
extern int XgetChunkStatus(int sockfd, char* dag, size_t dagLen);
extern int XgetChunkStatuses(int sockfd, ChunkStatus *statusList, int numCids);
extern int XreadChunk(int sockfd, void *rbuf, size_t len, int flags, char *cid, size_t cidLen);
extern int XpushChunkto(const ChunkContext* ctx, const char* buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo* info);
extern int XpushBufferto(const ChunkContext *ctx, const char *data, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize);
extern int XpushFileto(const ChunkContext *ctx, const char *fname, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize);
extern int XrecvChunkfrom(int sockfd, void* rbuf, size_t len, int flags, ChunkInfo* ci);
extern int XbindPush(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

extern ChunkContext *XallocCacheSlice(unsigned policy, unsigned ttl, unsigned size);
extern int XfreeCacheSlice(ChunkContext *ctx);
extern int XputChunk(const ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info);
extern int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **infoList);
extern int XputBuffer(ChunkContext *ctx, const char *, unsigned size, unsigned chunkSize, ChunkInfo **infoList);
extern int XremoveChunk(ChunkContext *ctx, const char *cid);
extern void XfreeChunkInfo(ChunkInfo *infoList);
#endif

extern void set_conf(const char *filename, const char *sectioname);
extern void print_conf();

extern int Xsetsockopt(int sockfd, int optname, const void *optval, socklen_t optlen);
extern int Xgetsockopt(int sockfd, int optname, void *optval, socklen_t *optlen);

extern int XgetNamebyDAG(char *name, int namelen, const sockaddr_x *addr, socklen_t *addrlen);
extern int XgetDAGbyName(const char *name, sockaddr_x *addr, socklen_t *addrlen);
extern int XregisterName(const char *name, sockaddr_x *addr);
extern int XrendezvousUpdate(const char *hidstr, sockaddr_x *DAG);

extern int XreadLocalHostAddr(int sockfd, char *localhostAD, unsigned lenAD, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID);
extern int XsetXcacheSID(int sockfd, char *, unsigned);

/* internal only functions */
extern int XupdateAD(int sockfd, char *newad, char *new4id);
extern int XupdateRV(int sockfd);
extern int XupdateNameServerDAG(int sockfd, char *nsDAG);
extern int XreadNameServerDAG(int sockfd, sockaddr_x *nsDAG);
extern int XisDualStackRouter(int sockfd);

extern int Xgetpeername(int sockd, struct sockaddr *addr, socklen_t *addrlen);
extern int Xgetsockname(int sockd, struct sockaddr *addr, socklen_t *addrlen);

extern int Xgetaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
extern int XreadRVServerAddr(char *, int);
extern int XreadRVServerControlAddr(char *, int);
extern void Xfreeaddrinfo(struct addrinfo *);
extern const char *Xgai_strerror(int);
extern int checkXid(const char *xid, const char *type);

extern char *XrootDir(char *buf, unsigned len);
extern void debug(int sock);
	
extern int getXcacheInPort(void);
extern int getXcacheOutPort(void);

#ifdef __cplusplus
}
#endif

#endif

