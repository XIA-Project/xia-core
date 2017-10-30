#ifndef __STORE_MANAGER_H__
#define __STORE_MANAGER_H__

#include <iostream>
#include <vector>
#include "stores/store.h"

/**
 * Efficiently manage various content stores.
 * xcache_store_manager keeps a list of available content stores (xcache_content_store).
 * On invocation of member function store(), store manager dynamically decides
 * which content store should be used to store this particular content object.
 */

class xcache_store_manager {
private:
	/* Vector of all the stores */
	std::vector<xcache_content_store *> store_vector;

public:
	xcache_store_manager();

	/**
	 * Select a content store to store this data and actually store it.
	 * @param meta:   Metadata of the object to be stored
	 * @param data:   Data to be stored.
	 * @returns >= 0: On successfully storing the data
	 * @returns < 0:  On failure
	 */
	int store(xcache_meta *meta, const std::string &data);
};

#endif
