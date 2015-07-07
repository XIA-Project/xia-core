#include "xcachePriv.h"
#include "xcache.h"

/* API */
int XcacheAllocateSlice(struct xcacheSlice *slice, int32_t cache_size, int32_t ttl, int32_t cache_policy)
{
	int ret;
	XcacheCommand cmd;

	cmd.set_cmd(XcacheCommand::XCACHE_NEWSLICE);
	cmd.set_ttl(ttl);
	cmd.set_cachesize(cache_size);
	cmd.set_cachepolicy(cache_policy);

	if((ret = send_command(&cmd)) < 0) {
		return ret;
	}

	if((ret = get_response_blocking(&cmd)) >= 0) {
		slice->contextId = cmd.contextid();
	} else {
		slice->contextId = -1;
	}

	return ret;
}
