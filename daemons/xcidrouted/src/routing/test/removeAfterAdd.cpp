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

static int numCIDs = 10;
static vector<string> cids;

static XIARouter xr;

void initCIDs(int offset){
	for(int i = 0; i < numCIDs; i++){
		string t = to_string(i+offset);
		string currCID = "CID:";
		for(int j = 0; j < 40 - (int)t.size(); j++){
			currCID += "0";
		}
		currCID += t;
		cids.push_back(currCID);
	}
}

int main(int argc, char const *argv[])
{
	int rc;
	string prev;
	XIARouter xr;

	if(argc < 2 || argc > 3){
		printf("invalid argument \n");
		printf("./removeAfterAdd hostname [numcids=10]\n");
		exit(-1);
	} else if(argc == 3){
		numCIDs = atoi(argv[2]);
	}

	printf("hostname: %s numcids: %d\n", argv[1], numCIDs);

	xr.setRouter(argv[1]);
	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		exit(-1);
	}

	initCIDs(0);
	for(int i = 0; i < numCIDs; i++){
		if(prev != ""){
			printf("deleting route localhost: %s\n", prev.c_str());
			rc = xr.delRoute(prev);
			printf("status code %d error message %s\n", rc, xr.cserror());
		}
	
		printf("setting route localhost: %s\n", cids[i].c_str());
		rc = xr.setRoute(cids[i], DESTINED_FOR_LOCALHOST, "", 0);
		printf("status code %d error message %s\n", rc, xr.cserror());
		
		prev = cids[i];
	}

	return 0;
}