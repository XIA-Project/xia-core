#include "slice.h"
#include "meta.h"
#include "stores/store.h"
#include "policies/policy.h"

#define IGNORE_PARAM(__param) ((void)__param)


// FIXME: HORRIBLE HACK to put this where the garbage collector can get at it.
// find a better way of making this happen
XIARouter xr;

void xcache_meta::init()
{
	_store = NULL;
	_policy = NULL;
	_len = 0;
	_ttl = 0;
	_initial_seq = 0;
	_fetchers = 0;
	pthread_mutex_init(&_meta_lock, NULL);
	_updated = _accessed = time(NULL);
}

xcache_meta::xcache_meta()
{
	init();
}

xcache_meta::xcache_meta(std::string cid)
{
	init();
	_cid = cid;
}

void xcache_meta::access() {
	_accessed = time(NULL);
	_policy->touch(this, false);
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
	syslog(LOG_INFO, "[%s] %s", _cid.c_str(), _store->get(this).c_str());
}

meta_map::meta_map()
{
	pthread_rwlock_init(&_rwlock, NULL);
}

meta_map::~meta_map()
{
	std::map<std::string, xcache_meta *>::iterator i;

	for (i = _map.begin(); i != _map.end(); i++) {
		delete i->second;
	}
	_map.clear();

	pthread_rwlock_destroy(&_rwlock);
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
	time_t now = time(NULL);

	for (i = _map.begin(); i != _map.end(); ) {
		xcache_meta *m = i->second;
		std::string c = "CID:" + m->get_cid();

		switch(m->state()) {
			case CACHING:
				// printf("walk:caching\n");
				// see if the chunk stalled for too long while caching
				if (now - m->updated() > TOO_OLD) {
					syslog(LOG_INFO, "removing stalled cid:%s", c.c_str());
					delete m;
					_map.erase(i++);
				} else {
					++i;
				}
				break;

			case AVAILABLE:
				// printf("walk:available\n");
				// see if it's time to live has expired
				if (m->ttl() == 0) {
					++i;
					break;

				} else {
					//printf("checking ttl\n");
					if ((now - m->created()) < m->ttl()) {
						++i;
						break;
					}
				}

				// set state to evicting in case there are peers accessing it
				m->set_state(EVICTING);
				// fall through

			case EVICTING:
				// printf("walk:evicting\n");
				// the chunk was timed out, or removed from outside
				//  first remove it from the policy engine
				m->policy()->remove(m);

				// fall through

			case PURGING:
				// printf("walk:purging\n");

				// the chunk was marked for removal by the policy engine

				// remove meta from the route table and stores
				if (m->fetch_count() > 0) {
					m->set_state(PURGING);
					syslog(LOG_INFO, "%s is in use, marked for future eviction\n", c.c_str());
					continue;
				}

				syslog(LOG_INFO, "%s chunk %s", (m->state() == EVICTING ? "evicting" : "purging"), c.c_str());
				xr.delRoute(c);
				m->store()->remove(m);
				delete m;
				_map.erase(i++);
				break;

			default:
				++i;
		}
	}

	unlock();
	return 0;
}
