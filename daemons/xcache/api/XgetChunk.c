#include <stdio.h>
#include "Xsocket.h"
#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XgetChunk(int sockfd, sockaddr_x *addr, socklen_t addrlen, void *rbuf, size_t buflen)
{
	xcache_cmd cmd;

	(void)sockfd; //FIXME: What does sockfd mean?

	printf("Inside %s\n", __func__);

	cmd.set_cmd(xcache_cmd::XCACHE_GETCHUNK);
	cmd.set_dag(addr, addrlen);

	if(send_command(&cmd) < 0) {
		printf("Error in sending command to xcache\n");
		/* Error in Sending chunk */
		return -1;
	}
	printf("Command sent to xcache successfully\n");

	printf("Waiting for a response from xcache\n");
	if(get_response_blocking(&cmd) >= 0) {
		/* Got a valid response from Xcache */
		memcpy(rbuf, cmd.data().c_str(), MIN(cmd.data().length(), buflen));
		printf("Got a valid response from xcache, chunk = %s, len = %lu\n",
			   cmd.data().c_str(), cmd.data().length());
		/* Got a valid response from Xcache */
		return MIN(cmd.data().length(), buflen);
	}
	printf("Did not get a valid response from xcache\n");

	return -1;
}

