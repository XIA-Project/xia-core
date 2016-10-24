#include "meta.h"
#include "policy_manager.h"

bool xcache_policy_manager::cacheable(xcache_meta *meta)
{
	int ret = -1;

	std::cout << "PolicyManager: Store\n";
	for(std::vector<xcache_eviction_policy *>::iterator i = policy_vector.begin(); i != policy_vector.end(); ++i) {
		ret = (*i)->store(meta);
	}

	return ret;
}

xcache_policy_manager::xcache_policy_manager()
{
	policy_vector.push_back(new LruPolicy());
}
