#ifndef __META_H__
#define __META_H__

#include "clicknet/xia.h"
#include <iostream>
#include <map>
#include <iostream>
#include <stdint.h>
#include "xcache_cmd.pb.h"
#include <unistd.h>
#include <time.h>


class xcache_content_store;

typedef enum {
	AVAILABLE,
	FETCHING,
	OVERHEARING,
	DENY_PENDING,
} chunk_states;

#define TOO_OLD 240

class xcache_meta {
private:
	chunk_states _state;

	pthread_mutex_t meta_lock;
	// Length of this content object.
	uint64_t len;

	// initial sequence number
	uint32_t initial_seq;

	xcache_content_store *store;
	std::string cid;
	std::string sid;

	time_t _accessed;
	time_t _updated;

	void init();

public:
	xcache_meta();
	xcache_meta(std::string);

	time_t last_accessed() { return _accessed; }
	void access() { _accessed = time(NULL); }

	time_t updated() { return _updated; }
	void update() { _updated = time(NULL); }

	// account for the tcp overhead in initial pkt
	void set_seq(uint32_t seq) { initial_seq = seq + 1; }
	uint32_t seq() { return initial_seq; }

	void set_dest_sid(std::string _sid) { sid = _sid; }
	std::string dest_sid() { return sid; }

	void set_state(chunk_states state) { _state = state;}
	chunk_states state() { return _state; }

	void set_store(xcache_content_store *s) { store = s; }

	void set_length(uint64_t length) { this->len = length; }

	// Actually read the content.
	std::string get(void);
	std::string get(off_t off, size_t len);
	std::string safe_get(void);

	bool is_stale();

	// Print information about the meta.
	void status(void);

	uint64_t get_length() { return len; }

	std::string get_cid() { return cid; }

	int lock(void) { return pthread_mutex_lock(&meta_lock); }
	int unlock(void) { return pthread_mutex_unlock(&meta_lock); }
};


class meta_map {
public:
	meta_map();
	~meta_map();

	int read_lock(void)  { return pthread_rwlock_wrlock(&rwlock); };
	int write_lock(void) { return pthread_rwlock_rdlock(&rwlock); };
	int unlock(void)     { return pthread_rwlock_unlock(&rwlock); };

	xcache_meta *acquire_meta(std::string cid);
	void release_meta(xcache_meta *meta);
	void add_meta(xcache_meta *meta);
	void remove_meta(xcache_meta *meta);

	//int walk(bool(*f)(xcache_meta *m));
	int walk();

private:
	std::map<std::string, xcache_meta *> _map;
	pthread_rwlock_t rwlock;
};

#endif
