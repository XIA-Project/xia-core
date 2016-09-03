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

static int NUM_CIDS = 500;
static int NUM_THREADS = 2;

static XIARouter xr;

vector<string> initCIDs(int offset){
	vector<string> cids;
	for(int i = 0; i < NUM_CIDS; i++){
		string t = to_string(i+offset);
		string currCID = "CID:";
		for(int j = 0; j < 40 - (int)t.size(); j++){
			currCID += "0";
		}
		currCID += t;
		cids.push_back(currCID);
	}

	return cids;
}

void setRoute(int n) {
	int rc;
	if(n == 0){
		vector<string> cids = initCIDs(0);
		// set localhost
		for(int i = 0; i < (int)cids.size(); i++){
			printf("setting route localhost: %s\n", cids[i].c_str());
			rc = xr.setRoute(cids[i], DESTINED_FOR_LOCALHOST, "", 0);
			printf("status code %d error message %s\n", rc, xr.cserror());
		}
	} else {
		vector<string> cids = initCIDs(NUM_CIDS);
		// set normal
		for(int i = 0; i < (int)cids.size(); i++){
			printf("setting route non-localhost: %s\n", cids[i].c_str());
			rc = xr.setRoute(cids[i], 1, cids[i], 0xffff);
			printf("status code %d error message %s\n", rc, xr.cserror());
		}
	}
}

int main() {
    int rc;

    xr.setRouter("router1");
	// connect to router
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click (%d)", rc);
		exit(-1);
	}

	thread t[NUM_THREADS];
   	for(int i=0; i < NUM_THREADS; i++){
    	t[i] = thread(setRoute, i);
   	}

   	for(int i=0; i < NUM_THREADS; i++){
    	t[i].join();
    }

    cout << "all threads terminated" << endl;

	return 0;
}