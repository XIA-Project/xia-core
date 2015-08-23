#ifndef __XCACHE_H__
#define __XCACHE_H__

#include "Xsocket.h"
#include <stddef.h>
#include <stdint.h>
#include "../common/xcache_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CIDLEN 512

typedef struct {
	char* cid;
	size_t cidLen;
	int status; // 1: ready to be read, 0: waiting for chunk response, -1: failed
} ChunkStatus;

typedef struct xcache_context {
	int contextID;
	int xcacheSock;
	int notifSock;
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
#define XCF_SKIPCACHE 0x4

#define XCF_METACHUNK 0x10
#define XCF_DATACHUNK 0x20

/**
 * XcacheHandleInit
 * Initializes XcacheHandle.
 */
int XcacheHandleInit(XcacheHandle *h); //DONE
int XputFile(XcacheHandle *h, const char *filename, size_t chunkSize, sockaddr_x **info);  //DONE
int XputChunk(XcacheHandle *h, const void *data, size_t length, sockaddr_x *info);  //DONE
int XputBuffer(XcacheHandle *h, const void *data, size_t length, size_t chunkSize, sockaddr_x **info);  //DONE
int XputMetaChunk(XcacheHandle *h, sockaddr_x *metachunk, sockaddr_x *addrs, socklen_t addrlen, int count); //DONE

int XbufInit(XcacheBuf *xbuf);
int XbufAdd(XcacheBuf *xbuf, void *data, size_t len);
int XbufPut(XcacheHandle *h, XcacheBuf *xbuf, size_t chunkSize, sockaddr_x **info);
void XbufFree(XcacheBuf *xbuf);

int XfetchChunk(XcacheHandle *h, void *buf, size_t buflen, int flags, sockaddr_x *addr, socklen_t addrlen);  //DONE

int XbufGetChunk(XcacheHandle *h, XcacheBuf *buf, sockaddr_x *addr, socklen_t addrlen, int *flags);

int XreadChunk(XcacheHandle *h, sockaddr_x *addr, socklen_t addrlen, void *buf, size_t len, off_t offset);  //DONE
int XchunkInit(XchunkHandle *, XcacheHandle *h, sockaddr_x *addr, socklen_t addrlen);
int XchunkRead(XchunkHandle *, void *buf, size_t len);

int XbufReadChunk(XcacheHandle *h, XcacheBuf *xbuf, sockaddr_x *addr, socklen_t addrlen);
int XregisterNotif(int event, void (*func)(XcacheHandle *, int event, sockaddr_x *addr, socklen_t addrlen));  //DONE
int XlaunchNotifThread(XcacheHandle *h);  //DONE
int XgetNotifSocket(XcacheHandle *h);
int XprocessNotif(XcacheHandle *h);

#ifdef __cplusplus
}
#endif

#endif
