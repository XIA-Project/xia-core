#include <sys/socket.h>
#include <sys/un.h>

#include "xcache.h"
#include "xcachePriv.h"
#include "../common/xcache_sock.h"

int XcacheInit(void)
{
	struct sockaddr_un xcache_addr;
	char sock_name[512];

	xcache_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(xcache_sock < 0)
		return -1;

	if(get_xcache_sock_name(sock_name, 512) < 0) {
		return -1;
	}

	/* Setup xcache's address */
	xcache_addr.sun_family = AF_UNIX;
	strcpy(xcache_addr.sun_path, sock_name);

	if(connect(xcache_sock, (struct sockaddr *)&xcache_addr, sizeof(xcache_addr)) < 0)
		return -1;

	return 0;
}
