#include "logger.h"
#include "slice.h"
#include "meta.h"
#include "policy.h"

DEFINE_LOG_MACROS(SLICE)

xcache_slice::xcache_slice(int32_t context_id)
{
	/* FIXME: Policy is always FIFO */
	FifoPolicy fifo;

	max_size = current_size = ttl = 0;
	policy = fifo;

	this->context_id = context_id;

}

void xcache_slice::add_meta(xcache_meta *meta)
{
	meta_map[meta->get_cid()] = meta;
	meta->added_to_slice(this);
}

bool xcache_slice::has_room(xcache_meta *meta)
{
	LOG_SLICE_INFO("Max = %Lu, Current = %Lu, Req = %Lu\n", max_size, current_size, meta->get_length());

	if(max_size - current_size >= meta->get_length())
		return true;
	return false;
}

void xcache_slice::remove_meta(xcache_meta *meta)
{
	std::map<std::string, xcache_meta *>::iterator iter;

	iter = meta_map.find(meta->get_cid());
	meta_map.erase(iter);
	meta->removed_from_slice(this);
}

void xcache_slice::make_room(xcache_meta *meta)
{
	while(!has_room(meta)) {
		xcache_meta *toRemove = policy.evict();
		if(toRemove)
			remove_meta(toRemove);
		// else
		// 	std::cout << "FIXME: Policy returns nothing to evict\n";
	}
}

bool xcache_slice::already_has_meta(xcache_meta *meta)
{
	std::map<std::string, xcache_meta *>::iterator iter;

	iter = meta_map.find(meta->get_cid());
	if(iter != meta_map.end())
		return true;
	return false;
}

int xcache_slice::store(xcache_meta *meta)
{
	if(already_has_meta(meta))
		return -1;

	make_room(meta);
	add_meta(meta);
	return policy.store(meta);
}

int xcache_slice::get(xcache_meta *meta)
{
	(void) meta;

	/* FIXME: Invoke policy functions */
	return 0;
}

void xcache_slice::set_policy(xcache_policy policy)
{
	this->policy = policy;
}

void xcache_slice::status(void)
{
	std::map<std::string, xcache_meta *>::iterator i;

	std::cout << "Slice [" << context_id << "]\n";
	for(i = meta_map.begin(); i != meta_map.end(); ++i) {
		i->second->status();
	}
}
