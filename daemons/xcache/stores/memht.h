#ifndef __MEMHT_H__
#define __MEMHT_H__

#include <map>
#include "store.h"

/**
 * MemHt:
 * @brief Content Store - [In-memory hash table]
 * In memory hash table using C++'s stl
 */

class MemHt:public xcache_content_store {
private:
	std::map<xcache_meta *, std::string> memht;

public:
	MemHt()
	{
	}

	int store(xcache_meta *meta, const std::string &data)
	{
		/* std::cout << "Reached MemHt::" << __func__ << " Storing " << data << "\n"; */
		memht[meta] = data;

		return 0;
	}

	std::string get(xcache_meta *meta)
	{
		/* std::cout << "Memht get\n"; */
		return memht[meta];
	}

	std::string get_partial(xcache_meta *meta, off_t off, size_t len)
	{
		return memht[meta].substr(off, len);
	}

	void print(void)
	{
    
	}
};

#endif
