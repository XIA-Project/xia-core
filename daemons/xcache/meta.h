#ifndef __META_H__
#define __META_H__

#include <iostream>
#include <map>
#include <iostream>
#include <stdint.h>
#include "headers/content_header.h"
#include "ncid_table.h"
#include "xcache_cmd.pb.h"
#include <unistd.h>
#include <time.h>

// FIXME: HORRIBLE HACK to put this where the garbage collector can get at it.
// find a better way of making this happen
#include "XIARouter.hh"
extern XIARouter xr;

class xcache_content_store;
class xcache_eviction_policy;


typedef enum {
	AVAILABLE,
	FETCHING,
	CACHING,
	EVICTING,
	READY_TO_SAVE,
	PURGING
} chunk_states;

#define TOO_OLD 240

class xcache_meta {
private:
	chunk_states _state;

	pthread_mutex_t _meta_lock;
	// Length of this content object.
	uint64_t _len;

	// initial sequence number
	uint32_t _initial_seq;

	xcache_content_store *_store;
	xcache_eviction_policy *_policy;

	std::string _sid;

	time_t _created;
	time_t _accessed;
	time_t _updated;

	int32_t _fetchers;

	ContentHeader *_chdr;

	void init();

public:
	xcache_meta();
	xcache_meta(ContentHeader *chdr);
	~xcache_meta();

	time_t last_accessed() { return _accessed; }
	void access();

	time_t updated() { return _updated; }
	void update() { _updated = time(NULL); }

	time_t created() { return _created; }
	void set_created() { _created = _updated = _accessed = time(NULL); }

	// account for the tcp overhead in initial pkt
	void set_seq(uint32_t seq) { _initial_seq = seq + 1; }
	uint32_t seq() { return _initial_seq; }

	void set_dest_sid(std::string sid) { _sid = sid; }
	std::string dest_sid() { return _sid; }

	void set_state(chunk_states state) { _state = state;}
	chunk_states state() { return _state; }

	void set_store(xcache_content_store *s) { _store = s; }
	xcache_content_store *store() { return _store; };

	void set_policy(xcache_eviction_policy *p) { _policy = p; }
	xcache_eviction_policy *policy() { return _policy; }

	void set_length(uint64_t length) { _len = length; }

	void set_ttl(time_t t) { _chdr->set_ttl(t); }
	time_t ttl() { return _chdr->ttl(); }

	void fetch(bool fetching) {
		if (fetching) {
			_fetchers++;
		} else {
			_fetchers--;
		}
		assert(_fetchers >= 0);
	}

	int32_t fetch_count() { return _fetchers; }

	// Actually read the content.
	std::string get(void);
	std::string get(off_t off, size_t len);
	std::string safe_get(void);

	bool is_stale();

	// Print information about the meta.
	void status(void);

	uint64_t get_length() { return _len; }

	std::string id() { return (_chdr) ? _chdr->id() : ""; }

	std::string store_id();
	std::vector<std::string> all_ids();

	bool valid_data(const std::string &data);

	void set_content_header(ContentHeader *chdr) { _chdr = chdr; }
	std::string content_header_str() {return _chdr->serialize();}

	int lock(void) {
		return 0;
		//return pthread_mutex_lock(&_meta_lock);
	}

	int unlock(void) {
		return 0;
		// return pthread_mutex_unlock(&_meta_lock);
	}
};


class meta_map {
public:
	static meta_map *get_map();

	xcache_meta *acquire_meta(std::string cid);
	void release_meta(xcache_meta *meta);
	void add_meta(xcache_meta *meta);
	void remove_meta(xcache_meta *meta);

	//int walk(bool(*f)(xcache_meta *m));
	int walk();

protected:
	meta_map();
	~meta_map();

private:
	int read_lock(void)  { return pthread_rwlock_rdlock(&_rwlock); };
	int write_lock(void) { return pthread_rwlock_wrlock(&_rwlock); };
	int unlock(void)     { return pthread_rwlock_unlock(&_rwlock); };

	std::map<std::string, xcache_meta *> _map;
	pthread_rwlock_t _rwlock;
	static meta_map* _instance;
	NCIDTable *_ncid_table;
};

#endif
