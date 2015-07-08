#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcachePutChunk(xcacheSlice *slice, xcacheChunk *chunk)
{
	XcacheCommand cmd;

	cmd.set_cmd(XcacheCommand::XCACHE_STORE);
	cmd.set_contextid(slice->contextId);
	cmd.set_data(chunk->buf, chunk->len);
	cmd.set_cid(std::string("0202020202020202020202020202020202020202"));
	strcpy(chunk->cid, "0202020202020202020202020202020202020202");

	return send_command(&cmd);
}
