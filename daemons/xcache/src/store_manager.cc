#include "meta.h"
#include "store_manager.h"

int XcacheStoreManager::store(XcacheMeta *meta, std::string data)
{
	int ret = -1;

	std::cout << "StoreManager: Store\n";
	for(std::vector<XcacheContentStore *>::iterator i = storeVector.begin(); i != storeVector.end(); ++i) {
		ret = (*i)->store(meta, data);
		if(ret >= 0) {
			meta->setStore(*i);
			break;
		}
	}

	return ret;
}

XcacheStoreManager::XcacheStoreManager()
{
	storeVector.push_back(new MemHt());
}
