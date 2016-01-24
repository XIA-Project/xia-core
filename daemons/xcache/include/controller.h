#ifndef __XCACHE_CONTROLLER_H__
#define __XCACHE_CONTROLLER_H__
#include <map>
#include <queue>
#include <iostream>
#include <semaphore.h>
#include "xcache_cmd.pb.h"
#include "slice.h"
#include "meta.h"
#include "store_manager.h"
#include "XIARouter.hh"
#include "cache.h"
#include "dagaddr.hpp"
#include <errno.h>
#include "api/xcache.h"

#define DEFAULT_THREADS 2
#define MAX_XID_SIZE 100

struct xcache_conf {
	char hostname[128];
	int threads;
};

struct xcache_req {
	int type;

#define XCFI_REMOVEFD 0x1

	int flags;
	int from_sock, to_sock;
	void *data;
	size_t datalen;
};

class xcache_controller {
private:
	sem_t req_sem;
	std::queue<xcache_req *> request_queue;
	pthread_mutex_t request_queue_lock;


	int n_threads;
	pthread_t *threads;

	/**
	 * Map of metadata.
	 */
	std::map<std::string, xcache_meta *> meta_map;
	pthread_mutex_t meta_map_lock;

	/**
	 * Map of contexts.
	 */
	std::map<uint32_t, struct xcache_context *> context_map;

	/**
	 * Hostname while running on localhost.
	 */
	std::string hostname;

	/**
	 * Lookup a context based on context ID.
	 */
	struct xcache_context *lookup_context(int);

	/**
	 * Manages various content stores.
	 * @See file store.h for details.
	 */
	xcache_store_manager store_manager;
	xcache_cache cache;

	XIARouter xr;
	int context_id;

public:
	char myAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE];
	char my4ID[MAX_XID_SIZE];

	/**
	 * A Constructor.
	 * FIXME: What do we need to do in the constructor?
	 */
	xcache_controller() {
		hostname.assign("host0");
		pthread_mutex_init(&meta_map_lock, NULL);
		pthread_mutex_init(&request_queue_lock, NULL);
		context_id = 0;
		sem_init(&req_sem, 0, 0);
	}

	/**
	 * Xcache Sender Thread.
	 * For sending content chunk, this thread keeps on "Accepting" connections
	 * and then sends appropriate content chunks to the receiver
	 */
	int create_sender(void);
	void send_content_remote(int sock, sockaddr_x *mypath);


	/**
	 * Configures Xcache.
	 * Based on command line parameters passed, this function configures xcache.
	 */
	void set_conf(struct xcache_conf *conf);

	/**
	 * Fetch content from remote Xcache.
	 * On reception of XgetChunk API, this function _MAY_ be invoked. If the
	 * chunk is found locally, then no need to fetch from remote.
	 */
	int fetch_content_remote(sockaddr_x *addr, socklen_t addrlen, xcache_cmd *,
							 xcache_cmd *, int flags);

	int fetch_content_local(sockaddr_x *addr, socklen_t addrlen, xcache_cmd *,
							xcache_cmd *, int flags);

	static void *__fetch_content(void *__args);

	int xcache_fetch_content(xcache_cmd *resp, xcache_cmd *cmd, int flags);

	/**
	 * Chunk reading
	 */
	int chunk_read(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * Handles commands received from the API.
	 */
	int fast_process_req(int fd, xcache_cmd *, xcache_cmd *);

	void process_req(struct xcache_req *req);

	static void *worker_thread(void *arg);


	/**
	 * Prints current xcache status.
	 */
	void status(void);

	/**
	 * Main controller entry / run function.
	 */
	void run(void);

	/** Xcache Command handlers **/
	/**
	 * Searches content locally (for now).
	 */
	int search(xcache_cmd *, xcache_cmd *);

	/**
	 * Stores content locally.
	 */
	int store(xcache_cmd *, xcache_cmd *);
	int __store(struct xcache_context *context, xcache_meta *meta, const std::string *data);
	int __store_policy(xcache_meta *);


	/**
	 * Allocate a new context.
	 */
	int alloc_context(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * Remove content.
	 */
	void remove(void);

	/** Concurrency control **/
	xcache_meta *acquire_meta(std::string cid);
	void release_meta(xcache_meta *meta);
	inline int lock_meta_map(void);
	inline int unlock_meta_map(void);

	int register_meta(xcache_meta *);
	void add_meta(xcache_meta *meta);
	int xcache_notify(struct xcache_context *c, sockaddr_x *addr,
					  socklen_t addrlen, int event);
	std::string addr2cid(sockaddr_x *addr);
	sockaddr_x cid2addr(std::string cid);

	void enqueue_request_safe(xcache_req *req);
	xcache_req *dequeue_request_safe(void);

	inline int req_sem_wait_safe(void) {
		while(sem_wait(&req_sem) != 0);

		return 0;
	}

	inline int req_sem_post_safe(void) {
		while(sem_post(&req_sem) != 0);

		return 0;
	}
};

#endif
