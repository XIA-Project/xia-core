#ifndef __META_H__
#define __META_H__

#include "clicknet/xia.h"
#include <iostream>
#include <map>
#include <iostream>
#include <stdint.h>
#include "store.h"
#include "xcache_cmd.pb.h"

class xcache_slice;

class xcache_meta {
private:
	/**
	 * Length of this content object.
	 */
	uint64_t len;

	/**
	 * Map of all the slices that store this meta.
	 */
	std::map<uint32_t, xcache_slice *> slice_map;
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
	xcache_meta(xcache_cmd *);

	/**
	 * Set Content store for this meta.
	 */
	void set_store(xcache_content_store *s) {
		store = s;
	}

	/**
	 * Adds passed slice to the slice_map.
	 * Must be called when a meta is added to slice.
	 */
	void added_to_slice(xcache_slice *slice);

	/**
	 * Removes slice from the map.
	 * Must be called whenever a meta is evicted from a slice.
	 */
	void removed_from_slice(xcache_slice *slice);

	void set_length(uint64_t length) {
		this->len = length;
	}

	/**
	 * Actually read the content.
	 */
	std::string get(void) {
		// FIXME: What about policy decisions?
		return store->get(this);
	}

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
};

#endif
