#ifndef __XCACHE_CONTROLLER_H__
#define __XCACHE_CONTROLLER_H__
#include <map>
#include <iostream>
#include "xcache_cmd.pb.h"
#include "slice.h"
#include "meta.h"
#include "store_manager.h"
#include "XIARouter.hh"

struct xcache_conf {
	char hostname[128];
};

/**
 * XcacheController:
 * @brief Abstract class that defines a eviction policy
 * Xcache eviction policy decides which content object to evict. The policy module
 * does not need to care about actually storing the data. It only deals in terms of
 * "meta"data.
 */

class XcacheController {
private:
	std::map<std::string, XcacheMeta *> metaMap;
	std::map<int32_t, XcacheSlice *> sliceMap;
	std::string hostname;

	int newSlice(XcacheCommand *, XcacheCommand *);
	XcacheSlice *lookupSlice(XcacheCommand *);
	XcacheStoreManager storeManager;
	XIARouter xr;

public:
	static void *startXcache(void *);

	XcacheController() {
	}

	void setConf(struct xcache_conf *conf) {
		hostname = std::string(conf->hostname);
	}

	int fetchContentRemote(XcacheCommand *, XcacheCommand *);
	void handleCli(void);
	void handleUdp(int);
	int handleCmd(XcacheCommand *, XcacheCommand *);
	void status(void);
	void run(void);

	int search(XcacheCommand *, XcacheCommand *);
	int store(XcacheCommand *);
	int storeFinish(XcacheMeta *, std::string);
	void remove(void);

};
#endif
