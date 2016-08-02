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

static vector<string> cids;

static int NUM_CIDS = 1000;
static int NUM_LOCAL_CIDS = 5;
static int NUM_THREADS = 2;

static char* HOST_NAME = "router1";

static XIARouter xr;

void initCIDs(){
	for(int i = 0; i < NUM_CIDS; i++){
		string t = to_string(i);
		string currCID = "CID:";
		for(int j = 0; j < 40 - (int)t.size(); j++){
			currCID += "0";
		}
		currCID += t;
		cids.push_back(currCID);
	}
}

void setRoute(int n) {
	int rc;
	XIARouter xr;

	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		return;
	}
	xr.setRouter(HOST_NAME);

	if(n == 0){
		// set localhost
		for(int i = 0; i < NUM_CIDS; i++){
			rc = xr.setRoute(cids[i], DESTINED_FOR_LOCALHOST, "", 0);
		}
	} else {
		// set normal
		for(int i = NUM_CIDS - 1; i >= 0; i--){
			rc = xr.setRoute(cids[i], 1, cids[i], 0xffff);
		}
	}
}

void getRouteEntries(string xidType, vector<XIARouteEntry> & result){
	if(result.size() != 0){
		result.clear();
	}

	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if(strcmp(r.xid.c_str(), "-") != 0){
				result.push_back(r);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	initCIDs();

	thread t[NUM_THREADS];
   	for(int i=0; i < NUM_THREADS; i++){
    	t[i] = thread(setRoute, i);
   	}

   	for(int i=0; i < NUM_THREADS; i++){
    	t[i].join();
    }

    int rc;
	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		exit(-1);
	}
	xr.setRouter(HOST_NAME);

    vector<XIARouteEntry> routeEntries;
	getRouteEntries("CID", routeEntries);

	int numLocal = 0;
	for(unsigned i = 0; i < routeEntries.size(); i++){
		if(routeEntries[i].port == (unsigned short)DESTINED_FOR_LOCALHOST){
			numLocal += 1;
		}
	}

	cout << "NUM_CIDS: " << NUM_CIDS << "  numLocal: " << numLocal << endl;

    cout << "all threads terminated" << endl;

	return 0;
}