#ifndef __POLICY_MANAGER_H__
#define __POLICY_MANAGER_H__

#include <iostream>
#include <vector>
#include "policies/policy.h"

/**
 * Efficiently manage various content policies.
 * xcache_policy_manager keeps a list of available content policies (xcache_content_policy).
 * On invocation of member function policy(), policy manager dynamically decides
 * which content policy should be used to policy this particular content object.
 */

class xcache_policy_manager {
private:
	/* Vector of all the policies */
	std::vector<xcache_eviction_policy *> policy_vector;

public:
	xcache_policy_manager();

	/**
	 * Select a content policy to policy this data and actually policy it.
	 * @param meta:    Metadata of the object to be cached
	 * @returns true:  The data can be stored
	 * @returns false: There is no room to cache the data
	 */
	bool cacheable(xcache_meta *meta);
};

#endif
