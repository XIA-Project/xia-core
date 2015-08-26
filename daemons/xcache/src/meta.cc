#include "slice.h"
#include "meta.h"

DEFINE_LOG_MACROS(META)

#define IGNORE_PARAM(__param) ((void)__param)

xcache_meta::xcache_meta(std::string cid)
{
	store = NULL;
	this->cid = cid;
	len = 0;
	pthread_mutex_init(&meta_lock, NULL);
}

std::string xcache_meta::get(void)
{
	return store->get(this);
}

std::string xcache_meta::get(off_t off, size_t len)
{
	return store->get_partial(this, off, len);
}

std::string xcache_meta::safe_get(void)
{
	std::string data;

	this->lock();
	data = get();
	this->unlock();

	return data;
}
xcache_meta::xcache_meta()
{
	store = NULL;
	pthread_mutex_init(&meta_lock, NULL);
}

void xcache_meta::status(void)
{
	std::cout << "[" << cid << "]\n";
	std::cout << "\tDATA: [" << store->get(this) << "]\n";
}

void xcache_meta::added_to_slice(xcache_slice *slice)
{
	slice_map[slice->get_context_id()] = slice;
}

void xcache_meta::removed_from_slice(xcache_slice *slice)
{
	// FIXME: Actually remove from this slice
	IGNORE_PARAM(slice);
}

