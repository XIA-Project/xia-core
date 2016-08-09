#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include "Xkeys.h"
#include "Xsocket.h"
#include "dagaddr.h"
#include "dagaddr.hpp"
#include "XIARouter.hh"

using namespace std;

static bool local = false;
static vector<string> cids;

static XIARouter xr;

vector<string> split_string_on_delimiter(char* str, char* delimiter){
    vector<string> result;

    char * pch;
    pch = strtok (str, delimiter);
    while (pch != NULL) {
        result.push_back(pch);
        pch = strtok (NULL, delimiter);
    }

    return result;
}

int interfaceNumber(string xidType, string xid) {
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			printf("r.xid: %s xid: %s\n", r.xid.c_str(), xid.c_str());
			if ((r.xid).compare(xid) == 0) {
				return (int)(r.port);
			}
		}
	}

	printf("rc: %d\n", rc);
	return -1;
}

int main(int argc, char *argv[])
{
	int rc;
	XIARouter xr;

	if(argc < 3 || argc > 4){
		printf("invalid argument \n");
		printf("./setCIDRoute hostname CID1-HID1,CID2-HID2 [local]\n");
		exit(-1);
	} else if (argc == 4){
		local = !strcmp(argv[3], "local");
	}

	cids = split_string_on_delimiter(argv[2], ",");

	xr.setRouter(argv[1]);
	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		exit(-1);
	}

	for(int i = 0; i < (int)cids.size(); i++){
		if(!local){
			vector<string> curr = split_string_on_delimiter((char*)cids[i].c_str(), "-");
			string cid = curr[0];
			string hid = curr[1];
			int port = interfaceNumber("HID", hid);

			printf("setting route cid: %s nexthop: %s port: %d\n", cid.c_str(), hid.c_str(), port);
			rc = xr.setRouteCIDRouting(cid, port, hid, 0xffff);
			printf("status code %d error message %s\n", rc, xr.cserror());
		} else {
			printf("setting route localhost: %s\n", cids[i].c_str());
			rc = xr.setRouteCIDRouting(cids[i], DESTINED_FOR_LOCALHOST, "", 0);
			printf("status code %d error message %s\n", rc, xr.cserror());
		}
	}

	return 0;
}