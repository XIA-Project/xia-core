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
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>

#include "xia.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXBUFLEN    62000	// Must be smaller than the MTU of localhost to allow for the protobuf and its contained data
							// this isn't calculated, so make sure more than enough room is available for now
#ifdef __mips__ // to add missing constants
	#define F_SETOWN_EX 15
	#define F_GETOWN_EX 16
	#define F_SETPIPE_SZ 1031
	#define F_GETPIPE_SZ 1032
	#define SO_REUSEPORT 15
	#define IPPROTO_BEETPH 94
	#define MSG_FASTOPEN 0x20000000
#endif


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

// Cache policy
//#define POLICY_LRU				0x00000001
//#define POLICY_FIFO				0x00000002
//#define POLICY_REMOVE_ON_EXIT	0x00001000
//#define POLICY_RETAIN_ON_EXIT	0x00002000
//#define POLICY_DEFAULT			(POLICY_LRU | POLICY_RETAIN_ON_EXIT)

#define CID_HASH_SIZE 40

#define DEFAULT_CHUNK_SIZE	2000

// XIA specific ifaddr flags
// must exceed last entry in enum net_device_flags in /usr/include/linux/if.h
#define XIFA_RVDAG 1<<25	// past /usr/include/linux/if.h net_device_flags

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
#define XPROTO_XCMP		0x01

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

extern unsigned XgetPrevAcceptHopCount();
extern int XacceptAs(int sockfd, struct sockaddr *remote_addr, socklen_t *remote_addrlen, struct sockaddr *addr, socklen_t *addrlen);
extern int Xaccept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
extern int Xbind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int Xconnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int Xpoll(struct pollfd *ufds, unsigned nfds, int timeout);
extern int Xppoll(struct pollfd *ufds, unsigned nfds, const struct timespec *tmo_p, const sigset_t *sigmask);
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
//extern int Xpselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, const struct timespec *timeout, const sigset_t *sigmask);

extern int Xfork(void);
extern int Xnotify(void);

extern int Xsetsockopt(int sockfd, int optname, const void *optval, socklen_t optlen);
extern int Xgetsockopt(int sockfd, int optname, void *optval, socklen_t *optlen);

extern int XgetNamebyDAG(char *name, int namelen, const sockaddr_x *addr);
extern int XgetDAGbyName(const char *name, sockaddr_x *addr, socklen_t *addrlen);
extern int XgetDAGbyAnycastName(const char *name, sockaddr_x *addr, socklen_t *addrlen);
extern int XregisterName(const char *name, sockaddr_x *addr);
extern int XregisterAnycastName(const char *name, sockaddr_x *DAG);

extern int XreadLocalHostAddr(int sockfd, char *localhostDAG, unsigned lenDAG, char *local4ID, unsigned len4ID);
extern int Xgethostname(char *name, size_t len);
extern int XsetXcacheSID(int sockfd, char *, unsigned);
extern int Xgetifaddrs(struct ifaddrs **ifap);
extern void Xfreeifaddrs(struct ifaddrs *ifa);

extern int XupdateDefaultInterface(int sockfd, int interface);
extern int XdefaultInterface(int sockfd);

extern int XmanageFID(const char *fid, bool create);
extern int XcreateFID(char *fid, int len);
extern int XremoveFID(const char *fid);

/* internal only functions */
extern int XupdateDAG(int sockfd, int interface, const char *rdag,
		const char *r4id, bool is_router);
extern int XupdateRV(int sockfd, int interface);
extern int XupdateNameServerDAG(int sockfd, const char *nsDAG);
extern int XreadNameServerDAG(int sockfd, sockaddr_x *nsDAG);
extern int XisDualStackRouter(int sockfd);
extern const char *XnetworkDAGIntentAD(const char *network_dag);

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

extern unsigned XmaxPayload();

extern const char *xia_ntop(int af, const sockaddr_x *src, char *dst, socklen_t size);
extern int xia_pton(int af, const char *src, sockaddr_x *dst);
extern size_t sockaddr_size(const sockaddr_x *s);

#ifdef __cplusplus
}
#endif

#endif
