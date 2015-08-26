#ifndef __XCACHE_CID_H__
#define __XCACHE_CID_H__

#include <stdio.h>

#include "Xsocket.h"

struct cid_header {
	size_t offset;
	size_t length;
	size_t total_length;
	char cid[CID_HASH_SIZE + 1];
	
} __attribute__((packed));

#endif
