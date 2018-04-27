#ifndef __NCID_TABLE_H__
#define __NCID_TABLE_H__

#include <iostream>
#include <map>
#include <algorithm>
#include <iostream>
#include <stdint.h>
#include "headers/content_header.h"
#include "xcache_cmd.pb.h"
#include <unistd.h>
#include <time.h>
#include "dagaddr.hpp"

class NCIDTable {
public:
	static NCIDTable *get_table();
	int register_ncid(std::string ncid, std::string cid);
	int unregister_ncid(std::string ncid, std::string cid);
	int unregister_cid(std::string cid);
	std::string get_known_cid(std::string content_id);
	std::string to_cid(std::string content_id);

protected:
	NCIDTable();
	~NCIDTable();

private:
	int read_lock(void)  { return pthread_rwlock_rdlock(&_rwlock); };
	int write_lock(void) { return pthread_rwlock_wrlock(&_rwlock); };
	int unlock(void)     { return pthread_rwlock_unlock(&_rwlock); };

	std::map<std::string, std::string> _ncid_to_cid;
	std::map<std::string, std::string>::iterator _ncid_to_cid_it;
	std::map<std::string, std::vector<std::string>> _cid_to_ncids;
	std::map<std::string, std::vector<std::string>>::iterator _cid_to_ncids_it;
	pthread_rwlock_t _rwlock;
	static NCIDTable* _instance;
};

#endif // __NCID_TABLE_H__
