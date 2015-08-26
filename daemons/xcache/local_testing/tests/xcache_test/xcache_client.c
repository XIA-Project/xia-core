#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>
#include "Xkeys.h"

#define MAX_XID_SIZE 100

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "exiting\n");
	exit(ecode);
}

int main(void)
{
	int sock;
	char CID[MAX_XID_SIZE] = "CID:0202020202020202020202020202020202020202";
//	char SID[MAX_XID_SIZE] = "SID:c839212ea24cc2e643ae317dd562faf5c98e4c89";
	sockaddr_x addr;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	if (XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0 )
		die(-1, "Reading localhost address\n");

	printf("%s, %s, %s\n", myAD, myHID, CID);
	
	Node n_src;
	Node n_ad(Node::XID_TYPE_AD, &myAD[3]);
	Node n_hid(Node::XID_TYPE_HID, &myHID[4]);
	Node n_cid(Node::XID_TYPE_CID, &CID[4]);
//	Node n_cid(Node::XID_TYPE_SID, &SID[4]);

	Graph primaryIntent = n_src * n_cid;
	Graph gFallback = n_src * n_ad * n_hid * n_cid;
	Graph gAddr = primaryIntent + gFallback;

	/* Graph primaryIntent = n_src * n_cid; */
	/* Graph gFallback = n_src * n_ad * n_hid; */
	/* Graph gAddr = gFallback; */

	gAddr.print_graph();
	gAddr.fill_sockaddr(&addr);

	if(Xconnect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		die(-1, "Xconnect failed\n");
	}

	return 0;
}
