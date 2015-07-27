#include <stdio.h>
#include "xcachePriv.h"
#include "xcache.h"

/* API */
ChunkContext *XallocCacheSlice(int32_t cache_size, int32_t ttl, int32_t cache_policy)
{
	int ret;
	xcache_cmd cmd;
	ChunkContext *ctx = NULL;

	printf("Lib: Sending Xcache Allocate Slice command\n");

	cmd.set_cmd(xcache_cmd::XCACHE_NEWSLICE);
	cmd.set_ttl(ttl);
	cmd.set_cache_size(cache_size);
	cmd.set_cache_policy(cache_policy);

	if((ret = send_command(&cmd)) < 0) {
		return NULL;
	}

	if((ret = get_response_blocking(&cmd)) >= 0) {
		ctx = (ChunkContext *)malloc(sizeof(ChunkContext));
		ctx->contextID = cmd.context_id();
	}

	return ctx;
}
