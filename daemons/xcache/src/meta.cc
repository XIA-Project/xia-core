#include "slice.h"
#include "meta.h"

DEFINE_LOG_MACROS(META)

#define IGNORE_PARAM(__param) ((void)__param)

xcache_meta::xcache_meta(std::string cid)
{
	store = NULL;
	this->cid = cid;
	len = 0;
}

std::string xcache_meta::get(void)
{

	std::map<uint32_t, xcache_slice *>::iterator i;

	for(i = slice_map.begin(); i != slice_map.end(); ++i) {
		xcache_slice *slice = i->second;

		slice->get(this);
	}

	return store->get(this);
}

xcache_meta::xcache_meta()
{
	store = NULL;
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

