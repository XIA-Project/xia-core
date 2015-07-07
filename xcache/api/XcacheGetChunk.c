#include "Xsocket.h"
#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcacheGetChunk(xcacheSlice *slice, xcacheChunk *chunk, sockaddr_x *addr, socklen_t len, int flags)
{
	XcacheCommand cmd;

	/* Flags currently unused */
	(void)flags;

	cmd.set_cmd(XcacheCommand::XCACHE_GETCHUNK);

	if(slice)
		cmd.set_contextid(slice->contextId);

	cmd.set_dag(addr, len);

	if(send_command(&cmd) < 0) {
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(&cmd) >= 0) {
		/* Got a valid response from Xcache */
		chunk->len = cmd.data().length();
		chunk->buf = malloc(chunk->len);
		memcpy(chunk->buf, cmd.data().c_str(), cmd.data().length());
		return 0;
	}

	return -1;
}

