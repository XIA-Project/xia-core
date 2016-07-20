#include <assert.h>
#include "Xsocket.h"
#include "xcache_sock.h"
#include "dagaddr.hpp"

#define XIDLEN 512
#define BUFFER 100

int get_xcache_sock_name(char *sockname, int len)
{
	char dag[XIA_MAX_DAG_STR_SIZE], fourid[XIDLEN];
	int sock = Xsocket(AF_XIA, SOCK_STREAM, 0);

	if(sock < 0) {
		printf("%s: Error while allocating socket\n", __func__);
		return -1;
	}

	if(XreadLocalHostAddr(sock, dag, sizeof(dag), fourid, XIDLEN) < 0) {
		printf("%s: Error while reading localhost addr\n", __func__);
		return -1;
	}

	Graph g(dag);
	char buf[64];
	strncpy(buf, g.intent_HID_str().c_str(), sizeof(buf));
	char *hid = strchr(buf, ':');

	if (hid == NULL) {
		printf("Unable to find HID to use in xcache sock name\n");
		return -1;
	}

	hid++;
	int rc = snprintf(sockname, len, "/tmp/xcache.%s", hid);
	printf("\n\n\nget_xcache_sock_name: %s\n\n\n", sockname);

	return rc;
}
