#include <stdio.h>
#include "Xsocket.h"
#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XcacheRequestChunk(xcacheSlice *slice, xcacheChunk *chunk, sockaddr_x *addr, socklen_t len, int flags)
{
	xcache_cmd cmd;

	/* Flags currently unused */
	(void)flags;

	printf("Inside %s\n", __func__);

	cmd.set_cmd(xcache_cmd::XCACHE_REQUESTCHUNK);

	if(slice)
		cmd.set_context_id(slice->context_id);

	cmd.set_dag(addr, len);

	if(send_command(&cmd) < 0) {
		printf("Error in sending command to xcache\n");
		/* Error in Sending chunk */
		return -1;
	}
	printf("Command sent to xcache successfully\n");

	return 0;
}
