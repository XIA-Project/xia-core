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
	_chdr = NULL;
	_store = NULL;
	_policy = NULL;
	_len = 0;
	_initial_seq = 0;
	_fetchers = 0;
	pthread_mutex_init(&_meta_lock, NULL);
	_updated = _accessed = time(NULL);
}

xcache_meta::xcache_meta()
{
	init();
}

xcache_meta::xcache_meta(ContentHeader *chdr)
{
	init();
	_chdr = chdr;
}

xcache_meta::~xcache_meta()
{
	if (_chdr != NULL) {
		delete _chdr;
	}
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

std::string xcache_meta::store_id()
{
	return _chdr->store_id();
}

std::vector<std::string> xcache_meta::all_ids()
{
	std::vector<std::string> ids;
	ids.push_back(store_id());
	if(store_id() != id()) {
		ids.push_back(id());
	}
	return ids;
}

bool xcache_meta::valid_data(const std::string &data)
{
	return _chdr->valid_data(data);
}

void xcache_meta::status(void)
{
	syslog(LOG_INFO,"[%s] %s", store_id().c_str(), _store->get(this).c_str());
}

// The only meta_map. Initialized by first call to meta_map::get_map()
meta_map* meta_map::_instance = 0;

meta_map::meta_map()
{
	_instance = 0;
	_ncid_table = NCIDTable::get_table();
	pthread_rwlock_init(&_rwlock, NULL);
}

/*!
 * @brief The only way to get a reference to the map storing all metadata
 */
meta_map* meta_map::get_map()
{
	if (_instance == 0) {
		_instance = new meta_map;
	}
	return _instance;
}

meta_map::~meta_map()
{
	std::map<std::string, xcache_meta *>::iterator i;

	for (i = _map.begin(); i != _map.end(); i++) {
		delete i->second;
	}
	_map.clear();

	pthread_rwlock_destroy(&_rwlock);
	delete _instance;
	_instance = 0;
}

xcache_meta *meta_map::acquire_meta(std::string cid)
{
	xcache_meta *m = NULL;

	// Sanity check user provided argument
	if(cid.size() < (CLICK_XIA_XID_ID_LEN*2)) {
		syslog(LOG_ERR, "Meta for invalid content id %s", cid.c_str());
		return NULL;
	}

	// If user requested NCID, convert to a CID
	std::string content_id = _ncid_table->to_cid(cid);

	read_lock();

	std::map<std::string, xcache_meta *>::iterator i = _map.find(content_id);
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
	// Metadata is only stored by CID
	_map[meta->store_id()] = meta;
	if(meta->store_id() != meta->id()) {
		_ncid_table->register_ncid(meta->id(), meta->store_id());
	}
	unlock();
}

void meta_map::remove_meta(xcache_meta *meta)
{
	// Note: this only removes the meta from the map, it doesn't delete it
	write_lock();
	_map.erase(meta->store_id());
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
		std::string id = m->id();
		std::string store_id = m->store_id();

		switch(m->state()) {
			case CACHING:
				// printf("walk:caching\n");
				// see if the chunk stalled for too long while caching
				if (now - m->updated() > TOO_OLD) {
					syslog(LOG_INFO, "removing stalled chunk:%s", id.c_str());
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
					syslog(LOG_INFO, "%s in use, marked for future eviction",
							id.c_str());
					continue;
				}

				syslog(LOG_INFO, "%s chunk %s", (m->state() == EVICTING ? "evicting" : "purging"), id.c_str());
				xr.delRoute(id);
				// If NCID, remove CID route and its entries from ncid table
				if(store_id != id) {
					xr.delRoute(store_id);
					_ncid_table->unregister_cid(store_id);
				}
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
