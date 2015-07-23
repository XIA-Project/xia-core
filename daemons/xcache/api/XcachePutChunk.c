#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcachePutChunk(xcacheSlice *slice, xcacheChunk *chunk)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_STORE);
	cmd.set_context_id(slice->context_id);
	cmd.set_data(chunk->buf, chunk->len);

	if(send_command(&cmd) < 0) {
		printf("%s: Error in sending command to xcache\n", __func__);
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(&cmd) < 0) {
		printf("Did not get a valid response from xcache\n");
		return -1;
	}

	if(cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
		if(cmd.status() == xcache_cmd::XCACHE_ERR_EXISTS) {
			printf("%s: Error this chunk already exists\n", __func__);
			return xcache_cmd::XCACHE_ERR_EXISTS;
		}
	}

	printf("%s: Got a response from server\n", __func__);
	strcpy(chunk->cid, cmd.cid().c_str());

	return xcache_cmd::XCACHE_OK;
}
