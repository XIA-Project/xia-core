#ifndef __STORE_H__
#define __STORE_H__

#include <iostream>
#include <sys/types.h>

#include "../meta.h"

/**
 * Abstract class that defines a content store.
 *
 * Content Store is a Xcache module that knows how to store and fetch
 * content.  For example, In-memory hash table could be one content
 * store that stores content in a hash table using cid as key and also
 * fetches it from the hash table. These stores are accessed by the
 * StoreManager after it decides to store content in one of these
 * stores.
 */

class xcache_content_store {
	/*
	 * FIXME Configuration parameters to be added here
	 */

public:
	/**
	 * Store method for the content store.
	 * @param meta: Metadata of the content to be stored
	 * @param data: Actual Data to be stored
	 * @returns >=0: On successfully storing content
	 * @returns <0: On failure
	 * Return values are crucial. These values are used by the
	 * store manager to take policy decisions.
	 */
	virtual int store(xcache_meta *meta, const std::string &data) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;
		(void)data;

		return 0;
	};

	/**
	 * Get method for the content store.
	 * @param meta: Key
	 * @returns data: Actual data. The store should read the
	 *                content in memory as a std::string and then
	 *                pass it back to the storeManager.
	 */
	virtual std::string get(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return NULL;
	};

	/**
	 * This get only reads a chunk partially.
	 * @param meta: Key
	 * @returns data: Actual data. The store should read the
	 *                content in memory as a std::string and then
	 *                pass it back to the storeManager.
	 */
	virtual std::string get_partial(xcache_meta *meta, off_t off,
					size_t len) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;
		(void)off;
		(void)len;

		return NULL;
	};

	/**
	 * Delete the content associated with meta from the content store.
	 * @param  meta Key
	 * @return      0 on success, -1 on error
	 */
	virtual int remove(xcache_meta *meta)
	{
		(void)meta;
		return 0;
	}

	/**
	 * Print the current status of the store.
	 * Hints:
	 * - Number of bytes allocated by the store
	 * - Occupied percentage
	 * - Average store latency / byte
	 */
	virtual void print(void) {
	};
};

/*
 * All the stores are implemented as separate c++ classes. The corresponding
 * header files must be included here.
 */
#include "memht.h"
#include "disk.h"

#endif
