#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdint.h>
#include <queue>
#include "policy.h"

class FifoPolicy:public xcache_eviction_policy {
	std::queue <xcache_meta *> q;

public:
	FifoPolicy() {
	}

	~FifoPolicy() {
	}

	int store(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		q.push(meta);

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
		xcache_meta *meta = q.front();

		if(meta)
			q.pop();

		return meta;
	}
};

#endif
