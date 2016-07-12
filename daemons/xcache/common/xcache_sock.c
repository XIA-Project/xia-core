#include "Xsocket.h"
#include "xcache_sock.h"
#include "dagaddr.hpp"

#define XIDLEN 512
#define BUFFER 100

int get_xcache_sock_name(char *sockname, int len)
{
	#define _HID "HID:"
	char dag[XIA_MAX_DAG_STR_SIZE], fourid[XIDLEN];
	const char *hid;
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
	const char *p = g.intent_HID_str().c_str();
	hid = strstr(p, _HID);
	if (hid == NULL) {
		hid = p;
	} else {
		hid += sizeof(_HID);
	}

	return snprintf(sockname, len, "/tmp/xcache.%s", hid);
}
