#include "Xsocket.h"
#include "xcache_sock.h"

#define XIDLEN 512
#define BUFFER 100

int get_xcache_sock_name(char *sockname, int len)
{
	char ad[XIDLEN], hid[XIDLEN], fourid[XIDLEN];
	int sock = Xsocket(AF_XIA, SOCK_STREAM, 0);

	if(sock < 0) {
		printf("%s: Error while allocating socket\n", __func__);
		return -1;
	}

	if(XreadLocalHostAddr(sock, ad, XIDLEN, hid, XIDLEN, fourid, XIDLEN) < 0) {
		printf("%s: Error while reading localhost addr\n", __func__);
		return -1;
	}

	return snprintf(sockname, len, "/tmp/xcache.%s", hid);
}
