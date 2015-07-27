#ifndef __XCACHE_H__
#define __XCACHE_H__

#include "Xsocket.h"
#include <stddef.h>
#include <stdint.h>

#define CIDLEN 512

/* CID cache context */
typedef struct {
    int sockfd; //FIXME: What does this mean?
    int contextID;
#if 0
	unsigned cachePolicy;
    unsigned cacheSize;
	unsigned ttl;
#endif
} ChunkContext;

typedef struct {
	int size;
	char cid[CID_HASH_SIZE + 1];
	int32_t ttl; //FIXME: unused
	struct timeval timestamp; //FIXME: unused
} ChunkInfo;

typedef struct {
	char* cid;
	size_t cidLen;
	int status; // 1: ready to be read, 0: waiting for chunk response, -1: failed
} ChunkStatus;


int Xinit(void);
int XgetChunk(int sockfd, sockaddr_x *addr, socklen_t addr_len, void *rbuf, size_t len);
ChunkContext *XallocCacheSlice(int32_t cache_size, int32_t ttl, int32_t cache_policy);
int XputChunk(ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info);
int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **chunks);

#endif
