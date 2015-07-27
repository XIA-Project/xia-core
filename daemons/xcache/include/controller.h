#ifndef __XCACHE_CONTROLLER_H__
#define __XCACHE_CONTROLLER_H__
#include <map>
#include <iostream>
#include "xcache_cmd.pb.h"
#include "slice.h"
#include "meta.h"
#include "store_manager.h"
#include "XIARouter.hh"
#include "cache.h"
#include <errno.h>

struct xcache_conf {
	char hostname[128];
};

class xcache_controller {
private:
	pthread_mutex_t meta_map_lock;

	/**
	 * Map of metadata.
	 */
	std::map<std::string, xcache_meta *> meta_map;	

	/**
	 * Map of slices.
	 */
	std::map<int32_t, xcache_slice *> slice_map;

	/**
	 * Hostname while running on localhost.
	 */
	std::string hostname;

	/**
	 * Allocate a new slice.
	 */
	int new_slice(xcache_cmd *, xcache_cmd *);

	/**
	 * Lookup a slice based on context ID.
	 */
	xcache_slice *lookup_slice(int);

	/**
	 * Manages various content stores.
	 * @See file store.h for details.
	 */
	xcache_store_manager store_manager;
	xcache_cache cache;

	XIARouter xr;
	int context_id;

public:
	/**
	 * A Constructor.
	 * FIXME: What do we need to do in the constructor?
	 */
	xcache_controller() {
		xcache_slice *slice = new xcache_slice(0);

#define DEFAULT_SLICE_TTL 10000
#define DEFAULT_SLICE_SIZE 2000000

		slice->set_ttl(DEFAULT_SLICE_TTL);
		slice->set_ttl(DEFAULT_SLICE_SIZE);
		slice_map[0] = slice;
		hostname.assign("host0");
		pthread_mutex_init(&meta_map_lock, NULL);
		
		context_id = 1;
	}

	/**
	 * Xcache Sender Thread.
	 * For sending content chunk, this thread keeps on "Accepting" connections
	 * and then sends appropriate content chunk to the receiver
	 */
	static void *start_xcache(void *);


	/**
	 * Configures Xcache.
	 * Based on command line parameters passed, this function configures xcache.
	 */
	void set_conf(struct xcache_conf *conf) {
		hostname = std::string(conf->hostname);
	}

	/**
	 * Fetch content from remote Xcache.
	 * On reception of XgetChunk API, this function _MAY_ be invoked. If the
	 * chunk is found locally, then no need to fetch from remote.
	 */
	int fetch_content_remote(xcache_cmd *, xcache_cmd *);

	/**
	 * Fetch content locally.
	 */
	int fetch_content_local(xcache_cmd *, std::string);

	/**
	 * Handles commands received from the API.
	 */
	int handle_cmd(xcache_cmd *, xcache_cmd *);

	/**
	 * Prints current xcache status (all the metas and slices).
	 */
	void status(void);

	/**
	 * Main controller entry / run function.
	 */
	void run(void);

	/**
	 * Searches content locally (for now).
	 */
	int search(xcache_cmd *, xcache_cmd *);

	/**
	 * Stores content locally.
	 */
	int store(xcache_cmd *, xcache_cmd *);

	/**
	 * Remove content.
	 */
	void remove(void);

	xcache_meta *acquire_meta(std::string cid) {
		lock_meta_map();

		std::map<std::string, xcache_meta *>::iterator i = meta_map.find(cid);
		if(i == meta_map.end()) {
			/* We could not find the content locally */
			unlock_meta_map();
			return NULL;
		}

		xcache_meta *meta = i->second;
		meta->lock();

		return meta;
	}

	void release_meta(xcache_meta *meta) {
		unlock_meta_map();
		if(meta)
			meta->unlock();
	}

	inline int lock_meta_map(void) {
		int rv;
//		std::cout << getpid() << " LOCKING METAMAP\n";
		rv = pthread_mutex_lock(&meta_map_lock);
		if(rv != 0) {
//			std::cout << "LOCKING FAILED " << errno << "\n";
		} else {
//			std::cout << getpid() << " LOCKED METAMAP\n";
		}
		return rv;
	}

	inline int unlock_meta_map(void) {
		int rv;
//		std::cout << getpid() << " UNLOCKING METAMAP\n";
		rv = pthread_mutex_unlock(&meta_map_lock);
		if(rv != 0) {
//			std::cout << "UNLOCKING FAILED " << errno << "\n";
		}
		return 0;
	}

	int register_meta(xcache_meta *);

	int __store(xcache_slice *slice, xcache_meta *meta, const std::string *data);

	void add_meta(xcache_meta *meta) {
		lock_meta_map();
		meta_map[meta->get_cid()] = meta;
		unlock_meta_map();
	}
};

#endif
