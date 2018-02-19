#ifndef __XCACHE_H__
#define __XCACHE_H__

#include "Xsocket.h"
#include <stddef.h>
#include <stdint.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#define NCID_MAX_STRLEN 1024

typedef struct {
	char* cid;
	size_t cidLen;
	int status; // 1: ready to be read, 0: waiting for chunk response, -1: failed
} ChunkStatus;

typedef struct xcache_context {
	int contextID;
	int xcacheSock;
	int notifSock;
	time_t ttl;
} XcacheHandle;

typedef struct {
	void *buf;
	size_t length;
} XcacheBuf;

typedef struct {
	int dummy;
} XchunkHandle;


/** FLAGS **/
#define XCF_BLOCK 0x1
#define XCF_DISABLENOTIF 0x2
#define XCF_CACHE 0x4

#define XCF_METACHUNK 0x10
#define XCF_DATACHUNK 0x20

/** EVENTS **/
enum {
	XCE_CHUNKARRIVED = 0,
	XCE_CHUNKAVAILABLE,
	XCE_MAX,
};

/**
 * XcacheHandleInit
 * Initializes XcacheHandle.
 */
int XcacheHandleInit(XcacheHandle *h);
int XcacheHandleDestroy(XcacheHandle *h);
int XcacheHandleSetTtl(XcacheHandle *h, time_t ttl);

extern int XputChunk(XcacheHandle *h, const char *data, size_t length,
		sockaddr_x *info);  //DONE
extern int XputNamedChunk(XcacheHandle *h, const char *data, size_t length,
		const char *content_name, const char *publisher_name);
extern int XputFile(XcacheHandle *h, const char *filename, size_t chunkSize, sockaddr_x **info);  //DONE
extern int XputBuffer(XcacheHandle *h, const void *data, size_t length, size_t chunkSize, sockaddr_x **info);  //DONE
extern int XputMetaChunk(XcacheHandle *h, sockaddr_x *metachunk, sockaddr_x *addrs, socklen_t addrlen, int count); //DONE

extern int XevictChunk(XcacheHandle *h, const char *cid);

extern int XbufInit(XcacheBuf *xbuf);
extern int XbufAdd(XcacheBuf *xbuf, void *data, size_t len);
extern int XbufPut(XcacheHandle *h, XcacheBuf *xbuf, size_t chunkSize, sockaddr_x **info);
extern void XbufFree(XcacheBuf *xbuf);

extern int XfetchNamedChunk(XcacheHandle *h, void **buf, int flags,
		const char *name);
extern int XfetchChunk(XcacheHandle *h, void **buf, int flags, sockaddr_x *addr, socklen_t addrlen);  //DONE
extern int _XfetchRemoteChunkBlocking(void **buf, sockaddr_x *addr, socklen_t len);
extern int XpushChunk(XcacheHandle *h, sockaddr_x *chunk, sockaddr_x *addr);
extern int XisChunkLocal(XcacheHandle *h, const char *chunk);

//extern int XbufGetChunk(XcacheHandle *h, XcacheBuf *buf, sockaddr_x *addr, socklen_t addrlen, int *flags);
//
//extern int XreadChunk(XcacheHandle *h, sockaddr_x *addr, socklen_t addrlen, void *buf, size_t len, off_t offset);  //DONE
//extern int XchunkInit(XchunkHandle *, XcacheHandle *h, sockaddr_x *addr, socklen_t addrlen);
//extern int XchunkRead(XchunkHandle *, void *buf, size_t len);

//extern int XbufReadChunk(XcacheHandle *h, XcacheBuf *xbuf, sockaddr_x *addr, socklen_t addrlen);
extern int XregisterNotif(int event, void (*func)(XcacheHandle *, int event, sockaddr_x *addr, socklen_t addrlen));  //DONE
extern int XlaunchNotifThread(XcacheHandle *h);  //DONE
extern int XnewProxy(XcacheHandle *h, std::string &proxyaddr);
extern int XgetNotifSocket(XcacheHandle *h);
extern int XprocessNotif(XcacheHandle *h);

#ifdef __cplusplus
}
#endif

#endif
