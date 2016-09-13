#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <algorithm>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include "Xsocket.h"
#include "dagaddr.hpp"
#include "xcidrouted_dv.hh"

#define DEFAULT_NAME "router0"
#define APPNAME "xcidrouted_dv"

static int ttl = -1;
static char *hostname = NULL;
static char *ident = NULL;

static XIARouter xr;
static RouteState route_state;

#if defined(STATS_LOG)
static Logger* logger;
#endif

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-t] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf(" -t TTL    	 : TTL for the CID advertisement, default is 1\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv)
{
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v:t:")) != -1) {
		switch (c) {
			case 'h':
				hostname = strdup(optarg);
				break;
			case 'l':
				level = MIN(atoi(optarg), LOG_DEBUG);
				break;
			case 'v':
				verbose = LOG_PERROR;
				break;
			case 't':
				ttl = atoi(optarg);
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname){
		hostname = strdup(DEFAULT_NAME);
	}

	if(ttl <= 0 || ttl > MAX_TTL){
		ttl = MAX_TTL;
	}

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void cleanup(int) {
#if defined(STATS_LOG)
	logger->end();
	delete logger;
#endif
}

void printRoutingTable(){
	syslog(LOG_INFO, "CID Routing table at %s", route_state.myHID);
  	for (auto it1=route_state.CIDrouteTable.begin() ; it1 != route_state.CIDrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d", (it1->first).c_str(), (it1->second.nextHop).c_str(), (it1->second.port));
  	}
}

void printNeighborTable(){
	syslog(LOG_NOTICE, "******************************* The neighbor table is: *******************************\n");
	set<std::string>::iterator it;
  	for (it = route_state.neighbors.begin(); it != route_state.neighbors.end(); it++ ) {
  		std::string neighborHID = *it;
  		syslog(LOG_NOTICE, "HID: %s", neighborHID.c_str());
   	}
}

void populateNeighborState(vector<XIARouteEntry> & currHidRouteEntries){
	for (auto i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(!eachEntry.nextHop.empty()){
			if(route_state.neighbors.find(eachEntry.nextHop) == route_state.neighbors.end()) {
				route_state.neighbors.insert(eachEntry.nextHop);
			}
		}
	}

	// then remove those entries that have element in map, but not in the HID route table
	vector<string> entriesToRemove;
  	for (auto it = route_state.neighbors.begin(); it != route_state.neighbors.end(); ++it) {
		bool found = false;
		string checkXid = *it;

		for (auto i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
			if((*i).nextHop == checkXid){
				found = true;
			}
		}

		if(!found){
			entriesToRemove.push_back(checkXid);
		}
  	}

  	for (auto i = entriesToRemove.begin(); i != entriesToRemove.end(); ++i) {
  		route_state.neighbors.erase(*i);
  	}
}

void populateRouteState(std::vector<XIARouteEntry> & routeEntries){
	for (auto i = routeEntries.begin(); i != routeEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(eachEntry.port == DESTINE_FOR_LOCALHOST){
			route_state.CIDrouteTable[eachEntry.xid].nextHop = route_state.myHID;
			route_state.CIDrouteTable[eachEntry.xid].port = DESTINE_FOR_LOCALHOST;
			route_state.CIDrouteTable[eachEntry.xid].cost = 0;
			route_state.CIDrouteTable[eachEntry.xid].timer = time(NULL);
		}
	}
}

int interfaceNumber(std::string xidType, std::string xid)
{
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if ((r.xid).compare(xid) == 0) {
				return (int)(r.port);
			}
		}
	}
	return -1;
}

void getRouteEntries(std::string xidType, std::vector<XIARouteEntry> & result){
	if(result.size() != 0){
		result.clear();
	}

	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		for (auto ir = routes.begin(); ir != routes.end(); ++ir) {
			XIARouteEntry r = *ir;
			if(strcmp(r.xid.c_str(), ROUTE_XID_DEFAULT) != 0){
				result.push_back(r);
			}
		}
	} else {
		syslog(LOG_ALERT, "unable to get routes from click (%d)", rc);
	}
}

double nextWaitTimeInSecond(double ratePerSecond){
	double currRand = (double)rand()/(double)RAND_MAX;
	double nextTime = -1*log(currRand)/ratePerSecond;	// next time in second
	return nextTime;	
}

size_t getTotalBytesForCIDRoutes(){
	size_t result = 0;

	for(auto it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it){
		result += it->first.size();
		result += it->second.nextHop.size();
		result += sizeof(it->second.port) + sizeof(it->second.cost) + sizeof(it->second.timer);
	}

	return result;
}

void initRouteState() {
	route_state.send_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
   	if (route_state.send_sock < 0) {
   		syslog(LOG_ALERT, "Unable to create a socket");
   		exit(-1);
   	}

   	route_state.recv_sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
   	if (route_state.recv_sock < 0) {
   		syslog(LOG_ALERT, "Unable to create a socket");
   		exit(-1);
   	}


	char cdag[MAX_DAG_SIZE];
	// read the localhost AD and HID
    if (XreadLocalHostAddr(route_state.recv_sock, cdag, MAX_DAG_SIZE, route_state.my4ID, MAX_XID_SIZE) < 0 ){
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}

    Graph g_localhost(cdag);
	strcpy(route_state.myAD, g_localhost.intent_AD_str().c_str());
    strcpy(route_state.myHID, g_localhost.intent_HID_str().c_str());

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai_recv;
	if (Xgetaddrinfo(NULL, SID_XROUTE_RECV, NULL, &ai_recv) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.sdag, ai_recv->ai_addr, sizeof(sockaddr_x));

	struct addrinfo *ai_send;
	if (Xgetaddrinfo(NULL, SID_XROUTE_SEND, NULL, &ai_send) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.ddag, ai_send->ai_addr, sizeof(sockaddr_x));


	// bind to the src DAG
   	if (Xbind(route_state.recv_sock, (struct sockaddr*)&route_state.sdag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&route_state.sdag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(route_state.recv_sock);
   		exit(-1);
   	}

	// bind to the dest DAG
   	if (Xbind(route_state.send_sock, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&route_state.ddag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(route_state.send_sock);
   		exit(-1);
   	}

   	printf("finished init route state\n");
}

void periodicJobs(){
	thread([](){
		vector<XIARouteEntry> currCidRouteEntries, currHidRouteEntries;

        while (true) {
            double nextTimeInSecond = nextWaitTimeInSecond(CID_ADVERT_UPDATE_RATE_PER_SEC);
            int nextTimeMilisecond = (int) ceil(nextTimeInSecond * 1000);

            route_state.mtx.lock();
            getRouteEntries("HID", currHidRouteEntries);
			populateNeighborState(currHidRouteEntries);

			getRouteEntries("CID", currCidRouteEntries);
			populateRouteState(currCidRouteEntries);
			broadcastRIP();

#ifdef STATS_LOG
			size_t totalSize = getTotalBytesForCIDRoutes();
			logger->log("size " + to_string(totalSize));
#endif
			route_state.mtx.unlock();

            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}

void updateClickRoutingTable() {
	int rc;

  	for (auto it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it) {
		if ((rc = xr.setRouteCIDRouting(it->first, it->second.port, it->second.nextHop, 0xffff)) != 0){
			syslog(LOG_ERR, "error setting route %d", rc);
		}
  	}
}

/* Message format (delimiter=^)
	num-source-cids
	source-CID1
	distance-cid1
	source-CID2
	distance-cid2
	...
*/
string constructBroadcastRIP(vector<string> & cids, string neighborHID){
	std::ostringstream sstream;
	
	int num_cids = 0;
	for (auto i = cids.begin(); i != cids.end(); ++i) {
		RouteEntry curr = route_state.CIDrouteTable[*i];
		
		// split horizon, don't advertise route learned from neighbor
		// back to neighbor
		if(curr.cost < ttl && curr.nextHop != neighborHID){
			num_cids++;
		}
	}
	sstream << num_cids << "^";

	if(num_cids != 0){
		for (auto i = cids.begin(); i != cids.end(); ++i) {
			RouteEntry curr = route_state.CIDrouteTable[*i];
			if(curr.cost < ttl && curr.nextHop != neighborHID){
				sstream << (*i) << "^";
				sstream << curr.cost << "^";
			}
		}	
	}

	return sstream.str();
}


int broadcastRIPHelper(const vector<string> & cids, string neighborHID, int start, int end){
	int rc1 = 0, rc2 = 0, msglen, buflen;
	char buffer[XIA_MAXBUF];
	bzero(buffer, XIA_MAXBUF);

	vector<string> temp;
	for(int i = start; i <= end; i++){
		temp.push_back(cids[i]);
	}

	string rip = constructBroadcastRIP(temp, neighborHID);
	msglen = rip.size();

	if(msglen < XIA_MAXBUF){
		strcpy (buffer, rip.c_str());
		buflen = strlen(buffer);

		sockaddr_x ddag;
		Graph g = Node() * Node(neighborHID) * Node(SID_XROUTE_RECV);
		g.fill_sockaddr(&ddag);

		rc1 = Xsendto(route_state.send_sock, buffer, buflen, 0, (struct sockaddr*)&ddag, sizeof(sockaddr_x));
		if(rc1 != buflen) {
			syslog(LOG_WARNING, "ERROR sending LSA. Tried sending %d bytes but rc=%d", buflen, rc1);
			return -1;
		}

#ifdef STATS_LOG
		logger->log("send " + to_string(buflen));
#endif

	} else {
		rc1 = broadcastRIPHelper(cids, neighborHID, start, (start + end)/2);
		rc2 = broadcastRIPHelper(cids, neighborHID, (start + end)/2 + 1, end);

		if(rc1 < 0 || rc2 < 0){
			return -1;
		}
	}

	return rc1 + rc2;
}

int broadcastRIP() {
	printf("Broadcast CID: \n");

	vector<string> allCIDs;
	for(auto it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it){
		allCIDs.push_back(it->first);
		printf("CID: %s\n", it->first.c_str());
		printf("\tnextHop: %s\n", it->second.nextHop.c_str());
		printf("\tcost: %d\n", it->second.cost);
	}

	if(allCIDs.size != 0){
		for (auto it = route_state.neighbors.begin(); it != route_state.neighbors.end(); it++) {
			string neighborHID = *it;
			printf("CIDs are sent to neighborHID: %s\n", neighborHID.c_str());
			if(broadcastRIPHelper(allCIDs, neighborHID, 0, allCIDs.size() - 1) < 0){
				syslog(LOG_WARNING, "cannot broad cast to neighborHID: %s\n", neighborHID.c_str());
			}
		}
	}


	return 0;
}

void removeOutdatedRoutes(){
	time_t now = time(NULL);
	vector<string> cidsToRemove;

  	for (auto it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it) {
  		if(now - it->second.timer >= EXPIRE_TIME){
  			xr.delRouteCIDRouting(it->first);
  			syslog(LOG_INFO, "purging CID route for : %s", it->first.c_str());
  			cidsToRemove.push_back(it->first);
  		}
  	}

  	for(auto it = cidsToRemove.begin(); it != cidsToRemove.end(); ++it){
  		route_state.CIDrouteTable.erase(*it);
  	}

#ifdef STATS_LOG
  	if(cidsToRemove.size() != 0){
		logger->log("route deletion");
  	}
#endif
}

int processRIPUpdate(string neighborHID, string rip_msg) {
	printf("Receive RIP from neighbor: %s\n", neighborHID.c_str());

	size_t found, start;
	string msg, cid_num_str, cid, cost;

	start = 0;
	msg = rip_msg;

  	int hid_interface = interfaceNumber("HID", neighborHID);

  	found = msg.find("^", start);
  	if (found != string::npos) {
  		cid_num_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	int num_cids = atoi(cid_num_str.c_str());

  	bool change = false;
  	for (int i = 0; i < num_cids; ++i) {
  		found = msg.find("^", start);
  		if (found != string::npos) {
  			cid = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		found = msg.find("^", start);
  		if (found != string::npos) {
  			cost = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

  		printf("\tcid: %s cost: %s\n", cid.c_str(), cost.c_str());

  		int curr_cost = atoi(cost.c_str());
  		if (route_state.CIDrouteTable.find(cid) != route_state.CIDrouteTable.end()){
  			if(route_state.CIDrouteTable[cid].cost > curr_cost + 1){
  				route_state.CIDrouteTable[cid].cost = curr_cost + 1;
  				route_state.CIDrouteTable[cid].port = hid_interface;
  				route_state.CIDrouteTable[cid].nextHop = neighborHID;
  				route_state.CIDrouteTable[cid].timer = time(NULL);
  				change = true;
  			} else if (route_state.CIDrouteTable[cid].cost == curr_cost + 1 && 
  					route_state.CIDrouteTable[cid].nextHop == neighborHID){
  				// refresh timer for CIDs.
  				route_state.CIDrouteTable[cid].timer = time(NULL);
  			}
  		} else {
  			route_state.CIDrouteTable[cid].cost = curr_cost + 1;
  			route_state.CIDrouteTable[cid].port = hid_interface;
  			route_state.CIDrouteTable[cid].nextHop = neighborHID;
  			route_state.CIDrouteTable[cid].timer = time(NULL);
  			change = true;
  		}

  		if(change){
  			printf("this route is set");
  		}
  	}

  	removeOutdatedRoutes();
  	if(change){
#ifdef STATS_LOG
		logger->log("route addition");
#endif
  		updateClickRoutingTable();
   	}

	return 1;
}

int main(int argc, char *argv[]) {
	int rc, selectRetVal, n, iteration = 0;
    socklen_t dlen;
    sockaddr_x theirDAG;
    fd_set socks;
    struct timeval timeoutval;
    char recv_message[XIA_MAXBUF];

    dlen = sizeof(sockaddr_x);

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

    // connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		return -1;
	}
	xr.setRouter(hostname);

   	initRouteState();

#if defined(STATS_LOG)
   	string namePrefix = hostname;
   	logger = new Logger(namePrefix + "_dv");
#endif
   	(void) signal(SIGINT, cleanup);

   	periodicJobs();

	while (1) {
		iteration++;
		FD_ZERO(&socks);
		FD_SET(route_state.recv_sock, &socks);

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000; // Main loop runs every 2000 usec

		selectRetVal = Xselect(route_state.recv_sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			memset(recv_message, 0, sizeof(recv_message));
			
			n = Xrecvfrom(route_state.recv_sock, recv_message, XIA_MAXBUF, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("Xrecvfrom have a problem");
			}
			
			Graph g(&theirDAG);
			string neighborHID = g.intent_HID_str();

			route_state.mtx.lock();
#ifdef STATS_LOG
			logger->log("recv " + to_string(n));
#endif		
			if(processRIPUpdate(neighborHID, recv_message) < 0){
				syslog(LOG_WARNING, "problem with processing RIP update\n");
			}
			route_state.mtx.unlock();
		}
    }

	return 0;
}