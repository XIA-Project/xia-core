#ifndef __PUBLISHER_LIST_H__
#define __PUBLISHER_LIST_H__

#include <map>
#include <pthread.h>
#include "publisher/publisher.h"

class PublisherList {
public:
	static PublisherList *get_publishers();
	Publisher *get(std::string publisher_name);

protected:
	PublisherList();
	~PublisherList();

private:
	int read_lock(void)  { return pthread_rwlock_rdlock(&_rwlock); };
	int write_lock(void) { return pthread_rwlock_wrlock(&_rwlock); };
	int unlock(void)     { return pthread_rwlock_unlock(&_rwlock); };

	std::map<std::string, Publisher*> _name_to_publisher;
	std::map<std::string, Publisher*>::iterator _name_to_publisher_it;
	// TODO: Add _publisher_to_name mappings so they can help with deletion
	pthread_rwlock_t _rwlock;
	static PublisherList* _instance;
};

#endif // __PUBLISHER_LIST_H__
