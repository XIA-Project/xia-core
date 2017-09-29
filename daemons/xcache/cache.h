#ifndef __CACHE_H__
#define __CACHE_H__

#include "cache_download.h"
#include "controller.h"
#include "cid.h"
/**
 * Opportunistic caching.
 */

#define CACHE_PORT 1444

#define PACKET_INVALID	-2
#define PACKET_NO_DATA	-1
#define PACKET_OK		 0

class xcache_cache;
class xcache_controller;

struct cache_args {
	xcache_cache *cache;
	xcache_controller *ctrl;
};

class xcache_cache {
private:
	static xcache_controller *ctrl;
	void blacklist(int cfsock, char *pkt, size_t len);
	// Ongoing downloads are keyed by source intent CID/NCID
	std::map<std::string, cache_download *> ongoing_downloads;

public:
	/**
	 * A Constructor.
	 */
	xcache_cache() {
	}

	void process_pkt(int cfsock, xcache_controller *ctrl,
			char *pkt, size_t len);
	int validate_pkt(char *pkt, size_t len, std::string &cid,
			std::string &sid, struct xtcp **xtcp);
	cache_download* start_new_download(struct xtcp *tcp,
			std::string cid, std::string sid);
	void unparse_xid(struct click_xia_xid_node *node, std::string &xid);


	static void *run(void *);
	static int create_click_socket();
	static void spawn_thread(struct cache_args *);
};


#endif
