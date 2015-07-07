#include <sys/socket.h>
#include <sys/un.h>

#include "xcache.h"
#include "xcachePriv.h"

int XcacheInit(void)
{
  struct sockaddr_un xcache_addr;

  xcache_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if(xcache_sock < 0)
    return -1;

  /* Setup xcache's address */
  xcache_addr.sun_family = AF_UNIX;
  strcpy(xcache_addr.sun_path, UNIX_SERVER_SOCKET);

  if(connect(xcache_sock, (struct sockaddr *)&xcache_addr, sizeof(xcache_addr)) < 0)
    return -1;

  return 0;
}
