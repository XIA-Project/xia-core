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

class xcache_meta {
private:
	enum chunk_states {
		AVAILABLE,
		FETCHING,
		OVERHEARING,
		DENY_PENDING,
	} state;

	pthread_mutex_t meta_lock;
	/**
	 * Length of this content object.
	 */
	uint64_t len;

	xcache_content_store *store;
	std::string cid;

public:
	/**
	 * A Constructor.
	 */
	xcache_meta();

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

#define DEFINE_STATE_MACROS(__state)					\
	bool is_##__state(void)  {					\
		return (state == (__state));				\
	}								\
	bool set_##__state(void) {					\
		std::cout << cid << " IS NOW " << #__state << "\n";	\
		return (state = (__state));				\
	}

DEFINE_STATE_MACROS(AVAILABLE)
DEFINE_STATE_MACROS(FETCHING)
DEFINE_STATE_MACROS(OVERHEARING)
DEFINE_STATE_MACROS(DENY_PENDING)

#undef DEFINE_STATE_MACROS
};

#endif
