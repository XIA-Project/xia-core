#include "meta.h"
#include "store_manager.h"

int xcache_store_manager::store(xcache_meta *meta, const std::string *data)
{
	int ret = -1;

	for(std::vector<xcache_content_store *>::iterator i = store_vector.begin();
            i != store_vector.end(); ++i) {
		ret = (*i)->store(meta, data);
		if(ret >= 0) {
			meta->set_store(*i);
			break;
		}
	}

	return ret;
}

std::string xcache_store_manager::get(xcache_meta *meta) {

	for(std::vector<xcache_content_store *>::iterator i = store_vector.begin();
            i != store_vector.end(); ++i) {
        return (*i)->get(meta);
	}

	return "";

}

xcache_store_manager::xcache_store_manager()
{
	store_vector.push_back(new MongodbStore());
}
