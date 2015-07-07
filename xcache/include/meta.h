#ifndef __META_H__
#define __META_H__

#include "../click-2.0.1/include/clicknet/xia.h"
#include <iostream>
#include <map>
#include <iostream>
#include <stdint.h>
#include "store.h"
#include "xcache_cmd.pb.h"

class XcacheSlice;

class XcacheMeta {
private:
	uint32_t refCount;
	uint64_t len;
	/* This map stores all the slices that this meta is a part of */
	std::map<uint32_t, XcacheSlice *> sliceMap;
	XcacheContentStore *store;
	std::string cid;

	void ref(void) {
		refCount++;
	}
	void deref(void) {
		refCount--;
	}

public:
	XcacheMeta();
	XcacheMeta(XcacheCommand *);
	void setStore(XcacheContentStore *s) {
		store = s;
	}
	void addedToSlice(XcacheSlice *slice);

	void removedFromSlice(XcacheSlice *slice);

	void setLength(uint64_t length) {
		this->len = length;
	}

	std::string get(void) {
		return store->get(this);
	}

	void status(void);

	uint64_t getLength() {
		return len;
	}

	/* For Debugging Purposes */
	void print(void) {
		std::cout << "RefCount = " << refCount << ", Length = " << len;
		if(store != NULL) {
			std::cout << ", Store = ";
			store->print();
		} else {
			std::cout << ", Store = NULL";
		}
		std::cout << std::endl;
	}

	std::string getCid() {
		return cid;
	}
};

#endif
