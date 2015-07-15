#include <stdio.h>
#include "xcachePriv.h"
#include "xcache.h"

/* API */
int XcacheAllocateSlice(struct xcacheSlice *slice, int32_t cache_size, int32_t ttl, int32_t cache_policy)
{
	int ret;
	xcache_cmd cmd;

	printf("Lib: Sending Xcache Allocate Slice command\n");

	cmd.set_cmd(xcache_cmd::XCACHE_NEWSLICE);
	cmd.set_ttl(ttl);
	cmd.set_cache_policy(cache_size);
	cmd.set_cache_policy(cache_policy);

	if((ret = send_command(&cmd)) < 0) {
		return ret;
	}

	if((ret = get_response_blocking(&cmd)) >= 0) {
		slice->context_id = cmd.context_id();
	} else {
		slice->context_id = -1;
	}

	return ret;
}
