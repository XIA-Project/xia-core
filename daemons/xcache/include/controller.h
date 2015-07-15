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

class xcache_controller {
private:
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
	xcache_slice *lookup_slice(xcache_cmd *);

	/**
	 * Manages various content stores.
	 * @See file store.h for details.
	 */
	xcache_store_manager store_manager;
	XIARouter xr;

public:
	/**
	 * A Constructor.
	 * FIXME: What do we need to do in the constructor?
	 */
	xcache_controller() {
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

	void handle_udp(int); /* Hopefully will go away */

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
	int store(xcache_cmd *);

	/**
	 * Remove content.
	 */
	void remove(void);
};
#endif
