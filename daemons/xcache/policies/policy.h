#ifndef __POLICY_H__
#define __POLICY_H__

#include "../meta.h"

/**
 * Abstract class that defines a eviction policy.
 * Xcache eviction policy decides which content object to evict. The policy module
 * does not need to care about actually storing the data. It only deals in terms of
 * "meta" data.
 */

class xcache_eviction_policy  {
public:
	virtual bool store(xcache_meta*) {
		return 0;
	};
	//virtual int get(xcache_meta*) {
	//	return 0;
	//};
	virtual bool remove(xcache_meta*) {
		return 0;
	};
	virtual bool evict(xcache_meta*) {
		return NULL;
	};
	virtual bool touch(xcache_meta*, bool = false) {
		return true;
	};
};

/*
 * All the policies are implemented c++ objects. The corresponding header files
 * must be included here.
 */
//#include "fifo.h"
#include "lru.h"

#endif /* __POLICY_H__ */
