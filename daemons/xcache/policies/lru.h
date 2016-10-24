#ifndef __LRU_H__
#define __LRU_H__

#include <stdint.h>
#include <list>
#include "policy.h"
#include "Xsocket.h"
#include "minIni.h"

class LruPolicy:public xcache_eviction_policy {
	std::list <xcache_meta *> q;

public:
	LruPolicy() {
		char config[256];
		XrootDir(config, sizeof(config));
		strcat(config, "/etc/xcache.ini");

		max_entries = ini_getl("lru", "max_entries", 0, config);
		max_size = ini_getl("lru", "max_size", 0, config);

		cache_size = 0.0;
	}

	bool pop() {
		bool rc = false;
		xcache_meta *m = q.back();
		if (m && m->state() == AVAILABLE) {
			m->lock();
			printf("\n\n:%lu : %f : Popping %s off the end", q.size(), cache_size, m->get_cid().c_str());

			cache_size -= m->get_length();
			m->set_state(EVICTING);
			q.pop_back();

			m->unlock();
			rc = true;
		}

		return rc;
	}

	int store(xcache_meta *meta) {
		bool has_room = true;

		q.push_front(meta);
		cache_size += meta->get_length();

		if (max_entries != 0) {
			while(q.size() > max_entries) {
				if (!pop()) {
					// FIXME: uh oh, what do we do in this case???
					has_room = false;
					break;
				}
			}
		}

		// FIXME: math danger!
		if (max_size != 0.0) {
			while(cache_size > max_size) {
				if (!pop()) {
					// FIXME: uh oh, what do we do in this case???
					has_room = false;
					break;
				}
			}
		}
		return has_room;
	}


	int touch(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */

		(void)meta;

		return 1;
	}
	int remove(xcache_meta *meta) {
		/* Ignoring compiler error for unused attribute */
		(void)meta;

		return 1;
	}
private:
	// upper limits for cache Storage
	// if 0, size is unlimited
	unsigned max_entries;
	unsigned max_size;

	double cache_size;
};

#endif
