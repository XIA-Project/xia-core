#ifndef __MONGODB_H__
#define __MONGODB_H__

#include "store.h"
#include "../meta.h"

class MongodbStore:public xcache_content_store {
public:
	MongodbStore();
	int store(xcache_meta *meta, const std::string *data);
	std::string get(xcache_meta *meta);
	std::string get_partial(xcache_meta *meta, off_t off, size_t len);
	int remove(xcache_meta *meta);
	void print(void);
};

#endif
