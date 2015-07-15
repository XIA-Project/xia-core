#ifndef __FIFO_H__
#define __FIFO_H__

#include "policy.h"

class FifoPolicy:public xcache_policy {
public:
	int store(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return 1;
	}
	int get(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return 1;
	}
	int remove(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return 1;
	}
	xcache_meta *evict() {
		return NULL;
	}
};

#endif
