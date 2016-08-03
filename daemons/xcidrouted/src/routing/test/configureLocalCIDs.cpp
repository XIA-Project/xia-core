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
static bool del = false;
static vector<string> cids;

static XIARouter xr;

void initCIDs(){
	for(int i = 0; i < numCIDs; i++){
		string t = to_string(i);
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
	XIARouter xr;

	if(argc < 2 || argc > 4){
		printf("invalid argument \n");
		exit(-1);
	} else if(argc == 3){
		numCIDs = atoi(argv[2]);
	} else if (argc == 4){
		del = !strcmp(argv[3], "del");
	}

	xr.setRouter(argv[1]);
	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		exit(-1);
	}

	initCIDs();
	for(int i = 0; i < numCIDs; i++){
		if(!del){
			printf("setting route localhost: %s\n", cids[i].c_str());
			rc = xr.setRouteCIDRouting(cids[i], DESTINED_FOR_LOCALHOST, "", 0);
			printf("status code %d error message %s\n", rc, xr.cserror());
		} else {
			printf("deleting route localhost: %s\n", cids[i].c_str());
			rc = xr.delRoute(cids[i]);
			printf("status code %d error message %s\n", rc, xr.cserror());
		}
	}

	return 0;
}