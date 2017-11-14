#ifndef __LRU_H__
#define __LRU_H__

#include <stdint.h>
#include <syslog.h>
#include <list>
#include "policy.h"
#include "Xsocket.h"
#include "minIni.h"

class LruPolicy:public xcache_eviction_policy {

public:
	LruPolicy() {
		char config[256];
		XrootDir(config, sizeof(config));
		strcat(config, "/etc/xcache.ini");

		_max_entries = ini_getl("lru", "_max_entries", 0, config);
		_max_size = ini_getl("lru", "_max_size", 0, config);
		_max_size *= 1024;

//		printf("cache policy = %lu %lu\n", _max_entries, _max_size);
		_cache_size = 0;
		pthread_mutex_init(&_lock, NULL);
	}


	// this will always return true unless an error has occurred
	bool store(xcache_meta *meta) {
		bool has_room = true;

		if (_max_entries == 0 && _max_size == 0) {
			// no need to do anything
			return true;
		}

		lock();
		// push the meta struct on the front of the queue
		_q.push_front(meta);
		_cache_size += meta->get_length();


		// FIXME: make this happen in the garbage collector instead of inline with inserts
		// if there are too many entries, pop them off the tail
		// until the size is acceptable
		if (_max_entries != 0 && _q.size() > _max_entries) {

			// purge the overage plus 10% more
			printf("checking max entries %lu\n", _q.size());
			while (_q.size() > (_max_entries - (_max_entries / 10))) {
				printf("cache has %lu, needs %lu\n", _q.size(), _max_entries - (_max_entries / 10));
				if (!pop()) {
					// FIXME: uh oh, what do we do in this case???
					has_room = false;
					break;
				}
				if (_q.size() == 1) {
					break;
				}
				printf("queue size is now %lu\n", _q.size());
			}
		}

//		printf("_max_size = %lu, current size = %lu\n", _max_size, _cache_size);
		if (_max_size != 0 && _cache_size > _max_size) {
			printf("checking bytes\n");
			size_t bytes = _max_size - (_max_size / 10);

			while(_cache_size > bytes) {
				printf("cache has %lu, needs %lu\n", _cache_size, bytes);
				if (!pop()) {
					// FIXME: uh oh, what do we do in this case???
					has_room = false;
					break;
				}
				if (_q.size() == 1) {
					break;
				}
			}
		}
		unlock();
		return has_room;
	}


	bool touch(xcache_meta *m, bool remove = false) {
		std::list <xcache_meta *>::iterator it;
		int rc = false;

		if (_max_entries == 0 && _max_size == 0) {
			// no need to do anything
			return true;
		}

		lock();
		for (it = _q.begin(); it != _q.end(); it++) {
			if (*it == m) {
				_q.erase(it);
				if (!remove) {
					_q.push_front(m);
				}
				rc = true;
				break;
			}
		}
		unlock();

		if (rc) {
			if (remove) {
				syslog(LOG_INFO, "LRU Policy: %s removed from queue\n", m->id().c_str());

			} else {
				syslog(LOG_INFO, "LRU Policy: %s moved back to front of queue\n", m->id().c_str());
			}

		} else {
			syslog(LOG_WARNING, "LRU Policy: %s not found\n", m->id().c_str());
		}
		return rc;
	}

	bool remove(xcache_meta *meta) {
		return touch(meta, true);
	}

private:
	std::list <xcache_meta *> _q;
	pthread_mutex_t _lock;

	// upper limits for cache Storage
	// if 0, size is unlimited
	size_t _max_entries;
	size_t _max_size;

	size_t _cache_size;

	int lock(void) {
		return pthread_mutex_lock(&_lock);
	}

	int unlock(void) {
		return pthread_mutex_unlock(&_lock);
	}

	bool pop() {
		bool rc = false;
		xcache_meta *m = _q.back();
		if (m) {
			switch (m->state()) {
				case AVAILABLE:
				case EVICTING:
					m->lock();
					printf("\n\nsz:%lu  bytes:%lu : Popping %s off the end\n", _q.size(), _cache_size, m->id().c_str());
					m->set_state(PURGING);
					m->unlock();

					_cache_size -= m->get_length();
					_q.pop_back();
					rc = true;
					break;

				case CACHING:
					_q.pop_back();
					_q.push_front(m);
					break;

				case PURGING:
				default:
					if (m->fetch_count() != 0) {
						// mark the meta for deletion
					}
					break;
			}
		}

		return rc;
	}
};

#endif
