#ifndef __STORE_MANAGER_H__
#define __STORE_MANAGER_H__

#include <iostream>
#include <vector>
#include "store.h"

/**
 * XcacheStoreManager:
 * @brief Efficiently manage various content stores
 * XcacheStoreManager keeps a list of available content stores (XcacheContentStore).
 * On invocation of member function store(), store manager dynamically decides
 * which content store should be used to store this particular content object.
 */

class XcacheStoreManager {
private:
	/* Vector of all the stores */
	std::vector<XcacheContentStore *> storeVector;

public:
	XcacheStoreManager();

	/**
	 * store():
	 * @brief Select a content store to store this data and actually store it.
	 * @param meta:   Metadata of the object to be stored
	 * @param data:   Data to be stored.
	 * @returns >= 0: On successfully storing the data
	 * @returns < 0:  On failure
	 */
	int store(XcacheMeta *meta, std::string data);
};

#endif
