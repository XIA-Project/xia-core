#ifndef __STORE_H__
#define __STORE_H__

#include <iostream>

class XcacheMeta;

/**
 * XcacheContentStore:
 * @brief Abstract class that defines a content store
 * Content Store is a Xcache module that knows how to store and fetch content.
 * For example, In-memory hash table could be one content store that stores content
 * in a hash table using cid as key and also fetches it from the hash table. These
 * stores are accessed by the StoreManager after it decides to store content in one
 * of these stores.
 */

class XcacheContentStore {
	/* TODO Configuration parameters to be added here */

public: 
	/**
	 * store():
	 * @brief Store method for the content store
	 * @param meta: Metadata of the content to be stored
	 * @param data: Actual Data to be stored
	 * @returns >=0: On successfully storing content
	 * @returns <0: On failure
	 * Return values are crucial. These values are used by the store manager to
	 * take policy decisions.
	 */
	virtual int store(XcacheMeta *meta, std::string data) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;
		(void)data;

		return 0;
	};

	/**
	 * get():
	 * @brief Get method for the content store
	 * @param meta: Key
	 * @returns data: Actual data. The store should read the content in memory as
	 *                a std::string and then pass it back to the storeManager.
	 */
	virtual std::string get(XcacheMeta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return NULL;
	};

	/**
	 * print():
	 * @brief Print the current status of the store.
	 * Hints:
	 * - Number of bytes allocated by the store
	 * - Occupied percentage
	 * - Average store latency / byte
	 */
	virtual void print(void) {
	};
};

/**
 * All the stores are implemented as header files. The corresponding header files
 * must be included here.
 */
#include "stores/memht.h"

#endif
