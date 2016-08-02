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

static XIARouter xr;
static vector<string> cids;

static int NUM_CIDS = 100;
static int NUM_LOCAL_CIDS = 5;
static int NUM_THREADS = 2;

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
	if(n == 0){
		// set localhost
		for(int i = 0; i < NUM_CIDS; i++){
			printf("set localhost route\n");
		}
	} else {
		// set normal
		for(int i = 0; i < NUM_CIDS; i++){
			printf("set normal route\n");
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

    cout << "all threads terminated" << endl;

	return 0;
}