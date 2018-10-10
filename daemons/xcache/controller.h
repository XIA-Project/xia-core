#ifndef __XCACHE_CONTROLLER_H__
#define __XCACHE_CONTROLLER_H__
#include <map>
#include <queue>
#include <mutex>
#include <iostream>
#include <semaphore.h>
#include "xcache_cmd.pb.h"
#include "slice.h"
#include "meta.h"
#include "ncid_table.h"
#include "store_manager.h"
#include "policy_manager.h"
#include "XIARouter.hh"
#include "cache.h"
#include "push_proxy.h"
#include "dagaddr.hpp"
#include <errno.h>
#include "xcache.h"


#define DEFAULT_THREADS 2
#define MAX_XID_SIZE 100
#define GC_INTERVAL 5

struct xcache_conf {
	char hostname[128];
	int threads;
};

struct xcache_req {
	int type;

#define XCFI_REMOVEFD 0x1
#define XCFI_CACHE     0x2

	int flags;
	time_t ttl;
	int from_sock, to_sock;
	char *cid;
	void *data;
	size_t datalen;
	unsigned hop_count;
};

class xcache_controller {
private:
	sem_t req_sem;
	std::queue<xcache_req *> request_queue;
	pthread_mutex_t request_queue_lock;
	pthread_mutex_t ncid_cid_lock;


	int n_threads;
	pthread_t *threads;

	meta_map *_map;
	NCIDTable *_ncid_table;
	std::map<int, std::thread *> proxy_threads;
	std::map<int, PushProxy *> push_proxies;
	int _proxy_id;
	std::mutex _push_proxies_lock;

	/**
	 * Map of contexts.
	 */
	std::map<uint32_t, struct xcache_context *> context_map;

	/**
	 * Map of NCID to CID
	 */
	std::map<std::string, std::string> ncid_to_cid;

	/**
	 * Map of CID to matching NCIDs
	 */
	std::map<std::string, std::vector<std::string>> cid_to_ncids;

	/**
	 * Hostname while running on localhost.
	 */
	std::string hostname;

	std::string xcache_sid;

	/**
	 * Lookup a context based on context ID.
	 */
	struct xcache_context *lookup_context(int);

	/**
	 * Check if the requested cid is in local storage
	 */
	bool is_content_local(std::string cid);

	/**
	 * Manages various content stores.
	 * @See file store.h for details.
	 */
	xcache_policy_manager policy_manager;
	xcache_store_manager store_manager;
	xcache_cache cache;

	unsigned context_id;

	/**
	 * The only controller instance
	 */
	static xcache_controller *_instance;

protected:
/**
	 * A Constructor.
	 * FIXME: What do we need to do in the constructor?
	 */
	xcache_controller();
	~xcache_controller();

public:
	static xcache_controller *get_instance();

	/**
	 * Xcache Sender Thread.
	 * For sending content chunk, this thread keeps on "Accepting" connections
	 * and then sends appropriate content chunks to the receiver
	 */
	int create_sender(void);
	void send_content_remote(xcache_req *req, sockaddr_x *mypath);

	bool verify_content(xcache_meta *meta, const std::string *);

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
	int fetch_content_remote(sockaddr_x *addr, socklen_t addrlen,
							 xcache_cmd *, int flags);

	int fetch_content_local(sockaddr_x *addr, socklen_t addrlen, xcache_cmd *,
							xcache_cmd *, int flags);

	static void *__fetch_content(void *__args);

	int xcache_fetch_content(xcache_cmd *resp, xcache_cmd *cmd, int flags);

	/**
	 * Simply check if the intent CID in the given DAG is local
	 */
	int xcache_is_content_local(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * Fetch named content from Xcache.
	 */
	int xcache_fetch_named_content(xcache_cmd *resp, xcache_cmd *cmd,
			int flags);

	/**
	 * Create a new proxy to cache pushed chunks
	 */
	int xcache_new_proxy(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * End a push proxy specified by the user
	 */
	int xcache_end_proxy(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * Send a chunk to a requested address
	 */
	int xcache_push_chunk(xcache_cmd *resp, xcache_cmd *cmd);

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
	static void *garbage_collector(void *);


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
	int store(std::string &cid, const std::string &data, time_t);
	int store(xcache_cmd *, xcache_cmd *, time_t);
	int __store(xcache_meta *meta, const std::string &data);
	int __store_policy(xcache_meta *);
	int store_named(xcache_cmd *, xcache_cmd *);

	// evict a chunk locally
	int evict(xcache_cmd *resp, xcache_cmd *cmd);

	/**
	 * Allocate a new context.
	 */
	int alloc_context(xcache_cmd *resp, xcache_cmd *cmd);

	// free the context
	int free_context(xcache_cmd *cmd);
	int free_context_for_sock(int sockfd);

	/** Check if a given CID is local **/
	bool is_CID_local(std::string cid);

	/**
	 * Remove content.
	 */
	void remove(void);

	/** Concurrency control **/
	xcache_meta *acquire_meta(std::string cid);
	void release_meta(xcache_meta *meta) { _map->release_meta(meta); };
	void add_meta(xcache_meta *meta) { _map->add_meta(meta); };
	void remove_meta(xcache_meta *meta) { _map->remove_meta(meta); };

	//inline int lock_meta_map(void);
	//inline int unlock_meta_map(void);

	int register_meta(std::vector<std::string> &ids);

	int xcache_notify(struct xcache_context *c, sockaddr_x *addr,
					  socklen_t addrlen, int event);
	int xcache_notify_contents(int context_id, const std::string &cid,
			const std::string &data);
	std::string addr2cid(sockaddr_x *addr);
	int cid2addr(std::string cid, sockaddr_x *sax);

	void enqueue_request_safe(xcache_req *req);
	xcache_req *dequeue_request_safe(void);
};

#endif
