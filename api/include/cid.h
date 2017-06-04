#ifndef __XCACHE_CID_H__
#define __XCACHE_CID_H__

#include <string>
#include "Xsocket.h"

#define CID_HEADER_VER 0x01

// options may eventually follow the header, their size should be
// included in hlen. but the code doesn't support this yet
// the format for options has not been determined yet either
struct cid_header {
	uint16_t version;
	uint16_t hlen;
	uint32_t length;
	uint32_t ttl;
	unsigned hop_count;
	//char cid[CID_HASH_SIZE];
} __attribute__((packed));

std::string compute_cid(const char *data, size_t len);

#endif
