#include "slice.h"
#include "meta.h"

#define IGNORE_PARAM(__param) ((void)__param)

xcache_meta::xcache_meta(xcache_cmd *cmd)
{
	store = NULL;
	cid = cmd->cid();
	len = cmd->cid().length();
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

