#ifndef __XCACHE_SLICE_H__
#define __XCACHE_SLICE_H__

#include <iostream>
#include <map>
#include "policies/policy.h"
#include <stdint.h>

class xcache_meta;
class xcache_eviction_policy;

class xcache_slice {
private:
	/**
	 * Map of metas that are a part of this slice.
	 */
	std::map<std::string, xcache_meta *> meta_map;

	/**
	 * Maximum allowed and current size of the slice.
	 * FIXME: Users should be able to configure the max_size.
	 */
	uint64_t max_size, current_size;

	/**
	 * Context ID for this slice.
	 * Every slice is identfied by the context id. The libxcache returns the
	 * context ID to the user. With every request the user application sends
	 * the context ID.
	 */
	int32_t context_id;

	/**
	 * Time to live for this slice.
	 * After TTL, all the metas are dereferenced from this slice. The metas
	 * which are part of *only* this slice, are evicted.
	 */
	uint64_t ttl;

	/**
	 * Eviction policy used by this slice/
	 */
	xcache_eviction_policy policy;

public:
	/**
	 * A constructor.
	 */
	xcache_slice(int32_t context_id);

	/* A few setattrs */
	void set_ttl(uint32_t ttl) {
		this->ttl = ttl;
	}

	void set_size(uint64_t size) {
		this->max_size = size;
	}

	void set_policy(xcache_eviction_policy);

	int32_t get_context_id(void) {
		return context_id;
	};

	/**
	 * Add a new meta in this slice.
	 */
	void add_meta(xcache_meta *);

	/**
	 * Callback indicating that user application invoked store method.
	 */
	int store(xcache_meta *);

	/**
	 * Callback indicating that user application invoked search method.
	 */
	int get(xcache_meta *);

	/**
	 * Remove this meta from this slice
	 */
	void remove_meta(xcache_meta *);

	/**
	 * Make room for passed meta.
	 * This function invokes policy object and asks for which objects to remove.
	 */
	void make_room(xcache_meta *);

	/**
	 * Print information about this slice.
	 */
	void status(void);

	/**
	 * Check if the meta is already present in the slice.
	 */
	bool already_has_meta(xcache_meta *);

	/**
	 * Find out if this slice has room for passed meta.
	 */
	bool has_room(xcache_meta *);
};

#endif
