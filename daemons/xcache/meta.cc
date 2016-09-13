#include "slice.h"
#include "meta.h"
#include "stores/store.h"

#define IGNORE_PARAM(__param) ((void)__param)

void xcache_meta::init()
{
	_store = NULL;
	len = 0;
	initial_seq = 0;
	pthread_mutex_init(&meta_lock, NULL);
	_updated = _accessed = time(NULL);
}

xcache_meta::xcache_meta()
{
	init();
}

xcache_meta::xcache_meta(std::string cid)
{
	init();
	this->cid = cid;
}

std::string xcache_meta::get(void)
{
	access();
	return _store->get(this);
}

std::string xcache_meta::get(off_t off, size_t len)
{
	access();
	return _store->get_partial(this, off, len);
}

std::string xcache_meta::safe_get(void)
{
	std::string data;

	this->lock();
	data = get();
	this->unlock();

	return data;
}

void xcache_meta::status(void)
{
	syslog(LOG_INFO, "[%s] %s", cid.c_str(), _store->get(this).c_str());
}

bool xcache_meta::is_stale()
{
	bool stale = false;

	if (state() == OVERHEARING) {
		if (time(NULL) - updated() > TOO_OLD) {
			stale = true;
		}
	}
	return stale;
}



meta_map::meta_map()
{
	pthread_rwlock_init(&rwlock, NULL);
}

meta_map::~meta_map()
{
	std::map<std::string, xcache_meta *>::iterator i;

	for (i = _map.begin(); i != _map.end(); i++) {
		delete i->second;
	}
	_map.clear();

	pthread_rwlock_destroy(&rwlock);
}

xcache_meta *meta_map::acquire_meta(std::string cid)
{
	xcache_meta *m = NULL;

	read_lock();

	std::map<std::string, xcache_meta *>::iterator i = _map.find(cid);
	if (i == _map.end()) {
		// We could not find the content locally
		unlock();
		return NULL;
	}

	m = i->second;
	m->lock();

	return m;
}

void meta_map::release_meta(xcache_meta *meta)
{
	if (meta) {
		meta->unlock();
	}

	unlock();
}

void meta_map::add_meta(xcache_meta *meta)
{
	write_lock();
	_map[meta->get_cid()] = meta;
	unlock();
}

void meta_map::remove_meta(xcache_meta *meta)
{
	// Note: this only removes the meta from the map, it doesn't delete it
	write_lock();
	_map.erase(meta->get_cid());
	unlock();
}

// delete any pending chunks that have stalled for too long
int meta_map::walk(void)
{
	write_lock();

	std::map<std::string, xcache_meta *>::iterator i;

	for (i = _map.begin(); i != _map.end(); ) {
		xcache_meta *m = i->second;

		if (m->is_stale()) {
			syslog(LOG_INFO, "removing stalled stream for %s", m->get_cid().c_str());
			delete m;
			_map.erase(i++);

		} else if (m->state() == EVICTING) {
			syslog(LOG_INFO, "evicting chunk %s", m->get_cid().c_str());
			m->store()->remove(m);
			delete m;
			_map.erase(i++);
		} else {
			++i;
		}
	}

	unlock();
	return 0;
}
