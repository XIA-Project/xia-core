#include "meta.h"
#include "policy_manager.h"

bool xcache_policy_manager::cacheable(xcache_meta *meta)
{
	bool ret = false;

	// FIXME: this logic doesn't make a lot of sense
	for(std::vector<xcache_eviction_policy *>::iterator i = policy_vector.begin(); i != policy_vector.end(); ++i) {
		meta->set_policy(*i);
		ret = (*i)->store(meta);
	}

	return ret;
}

xcache_policy_manager::xcache_policy_manager()
{
	policy_vector.push_back(new LruPolicy());
}
