#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcachePutChunk(xcacheSlice *slice, xcacheChunk *chunk)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_STORE);
	cmd.set_context_id(slice->context_id);
	cmd.set_data(chunk->buf, chunk->len);
	cmd.set_cid(std::string("0202020202020202020202020202020202020202"));
	strcpy(chunk->cid, "0202020202020202020202020202020202020202");

	return send_command(&cmd);
}
