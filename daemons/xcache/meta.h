#ifndef __META_H__
#define __META_H__

#include "clicknet/xia.h"
#include <iostream>
#include <map>
#include <iostream>
#include <stdint.h>
#include "xcache_cmd.pb.h"
#include <unistd.h>


class xcache_content_store;

typedef enum {
	AVAILABLE,
	FETCHING,
	OVERHEARING,
	DENY_PENDING,
} chunk_states;

class xcache_meta {
private:
	chunk_states _state;

	pthread_mutex_t meta_lock;
	/**
	 * Length of this content object.
	 */
	uint64_t len;

	// initial sequence number
	uint32_t initial_seq;

	xcache_content_store *store;
	std::string cid;
	std::string sid;
public:
	/**
	 * A Constructor.
	 */
	xcache_meta();

	void set_seq(uint32_t seq) {
		// account for the tcp overhead in initial pkt
		initial_seq = seq + 1;
	}

	uint32_t seq() {
		return initial_seq;
	}

	void set_dest_sid(std::string _sid) {
		sid = _sid;
	}

	std::string dest_sid() {
		return sid;
	}

	void set_state(chunk_states state) {
		_state = state;
	}

	chunk_states state() {
		return _state;
	}

	/**
	 * Another Constructor.
	 */
	xcache_meta(std::string);

	/**
	 * Set Content store for this meta.
	 */
	void set_store(xcache_content_store *s) {
		store = s;
	}

	void set_length(uint64_t length) {
		this->len = length;
	}

	/**
	 * Actually read the content.
	 */
	std::string get(void);

	/**
	 * Actually read the content.
	 */
	std::string get(off_t off, size_t len);

	/**
	 * Actually read the content.
	 */
	std::string safe_get(void);

	/**
	 * Print information about the meta.
	 */
	void status(void);

	uint64_t get_length() {
		return len;
	}

	std::string get_cid() {
		return cid;
	}

	int lock(void) {
		int rv;
//		std::cout << getpid() << " META LOCK " << cid << "\n";
		rv = pthread_mutex_lock(&meta_lock);
//		std::cout << getpid() << " META LOCKED " << cid << "\n";
		return rv;
	}

	int unlock(void) {
		int rv;
//		std::cout << getpid() << " META UNLOCK " << cid << "\n";
		rv = pthread_mutex_unlock(&meta_lock);
//		std::cout << getpid() << " META UNLOCKED " << cid << "\n";
		return rv;
	}
};

#endif
