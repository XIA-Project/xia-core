#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcachePutChunk(xcacheSlice *slice, xcacheChunk *chunk)
{
	XcacheCommand cmd;

	cmd.set_cmd(XcacheCommand::XCACHE_STORE);
	cmd.set_contextid(slice->contextId);
	cmd.set_data(chunk->buf, chunk->len);
	cmd.set_cid(std::string("CID:TODO"));
	strcpy(chunk->cid, "CID:TODO");

	return send_command(&cmd);
}
