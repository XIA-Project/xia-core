#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include "xcache.h"
#include "Xkeys.h"
#include "Xsocket.h"
#include "dagaddr.h"
#include "dagaddr.hpp"

#define MB(__mb) (KB(__mb) * 1024)
#define KB(__kb) ((__kb) * 1024)
#define CHUNKSIZE MB(1)

using namespace std;

Graph cid2addr(std::string CID, std::string AD, std::string HID) {
    Node n_src;
    Node n_cid(Node::XID_TYPE_CID, strchr(CID.c_str(), ':') + 1);
    Node n_ad(Node::XID_TYPE_AD, strchr(AD.c_str(), ':') + 1);
    Node n_hid(Node::XID_TYPE_HID, strchr(HID.c_str(), ':') + 1);

    Graph primaryIntent = n_src * n_cid;
    Graph gFallback = n_src * n_ad * n_hid * n_cid;
    Graph gAddr = primaryIntent + gFallback;

    return gAddr;
}

int main(int argc, char *argv[]){
	if(argc != 4){
		printf("must have AD HID CID\n");
		exit(-1);
	}

	sockaddr_x addr;
	Graph g = cid2addr(argv[1], argv[2], argv[3]);
	g.fill_sockaddr(&addr);

	XcacheHandle xcache;
	XcacheHandleInit(&xcache);

	int ret;
	char buf[CHUNKSIZE];
	if ((ret = XfetchChunk(&xcache, buf, CHUNKSIZE, XCF_BLOCK, &addr,
				       sizeof(addr))) < 0) {
		printf("Xfetch failed\n");
		exit(-1);
	}

	printf("xfetch success!\n");

	return 0;
}