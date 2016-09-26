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
#include "xcidrouted_ls.hh"

#define DEFAULT_NAME "router0"
#define APPNAME "xcidrouted"

static char *hostname = NULL;
static char *ident = NULL;

static int ttl = -1;
static XIARouter xr;
static RouteState route_state;

#if defined(STATS_LOG)
static Logger* logger;
#endif

void getRouteEntries(std::string xidType, std::vector<XIARouteEntry> & result){
	if(result.size() != 0){
		result.clear();
	}

	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if(strcmp(r.xid.c_str(), ROUTE_XID_DEFAULT) != 0){
				result.push_back(r);
			}
		}
	} else {
		syslog(LOG_ALERT, "unable to get routes from click (%d)", rc);
	}
}

int interfaceNumber(std::string xidType, std::string xid){
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

double nextWaitTimeInSecond(double ratePerSecond){
	double currRand = (double)rand()/(double)RAND_MAX;
	double nextTime = -1*log(currRand)/ratePerSecond;	// next time in second
	return nextTime;	
}

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-h hostname] [-t TTL]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf(" -t TTL    	 : TTL for the CID advertisement, default is 1\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv) {
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

void printNeighborTable(){
	syslog(LOG_NOTICE, "******************************* The neighbor table is: *******************************\n");
	map<std::string, NeighborEntry>::iterator it;
  	for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++ ) {
  		string neighborHID = it->first;
  		NeighborEntry entry = it->second;
  		syslog(LOG_NOTICE, "HID: %s", neighborHID.c_str());
  		syslog(LOG_NOTICE, "entry cost: %d", entry.cost);
  		syslog(LOG_NOTICE, "entry port: %d", entry.port);
   	}
}

void printNetworkTable(){
	syslog(LOG_NOTICE, "******************************* The network table is: *******************************\n");
	map<std::string, NodeStateEntry>::iterator it;
	set<int>::iterator it2;
  	for (it = route_state.networkTable.begin(); it != route_state.networkTable.end(); it++ ) {
  		NodeStateEntry entry = it->second;
  		syslog(LOG_NOTICE, "destination hid key: %s", it->first.c_str());
  		syslog(LOG_NOTICE, "seq number entry: %u", entry.seq);
  		for (it2 = entry.cid_nums.begin(); it2 != entry.cid_nums.end(); ++it2) {
  			syslog(LOG_NOTICE, "cid nums: %d", *it2);
  		}

  		syslog(LOG_NOTICE, "num dest cids: %lu\n", entry.dest_cids.size());
  		syslog(LOG_NOTICE, "destination num neighbors entry: %d", entry.num_neighbors);
  		vector<std::string>::iterator it2;
  		for (it2 = entry.neighbor_hids.begin(); it2 != entry.neighbor_hids.end(); it2++ ) {
  			syslog(LOG_NOTICE, "dest neighbor hid: %s", (*it2).c_str());
  		}
  	}
}

void printRoutingTable(){
	syslog(LOG_INFO, "CID Routing table at %s", route_state.myHID);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.CIDrouteTable.begin() ; it1 != route_state.CIDrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port));
  	}
}

void cleanup(int) {
#if defined(STATS_LOG)
	logger->end();
	delete logger;
#endif

	exit(0);
}

size_t getTotalBytesForCIDRoutes(){
	size_t result = 0;

	// get the space needed for storing the source CIDs
	for(auto it = route_state.sourceCids.begin(); it != route_state.sourceCids.end(); ++it){
		result += it->size();
	}

	// get the space needed for storing the network table
	for(auto it = route_state.networkTable.begin(); it != route_state.networkTable.end(); ++it){
		result += it->first.size();
		result += sizeof(it->second.seq);
		result += sizeof(int) * it->second.cid_nums.size();
		result += sizeof(it->second.num_neighbors);

		for(auto ij = it->second.dest_cids.begin(); ij != it->second.dest_cids.end(); ++ij){
			result += ij->first.size();
			result += sizeof(ij->second);
		}
		
		for(auto ij = it->second.neighbor_hids.begin(); ij != it->second.neighbor_hids.end(); ++ij){
			result += ij->size();
		}

		result += sizeof(it->second.checked) + sizeof(it->second.cost) + it->second.prevNode.size();	
	}

	// get the space needed for storing the CID route table
	for(auto it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it){
		result += it->first.size();
		result += it->second.dest.size() + it->second.nextHop.size();
		result += sizeof(it->second.port) + sizeof(it->second.cost);
	}

	return result;
}

void removeAbnormalNetworkTable(){
	vector<string> entryToRemove;
 	// remove entries in our table that is invalid:
 	// 1. zero neighbors. 2. empty dest HID.
  	for (auto i = route_state.networkTable.begin(); i != route_state.networkTable.end(); ++i) {
 		if((i->first).empty() || (i->second.num_neighbors == 0)) {
 			entryToRemove.push_back(i->first);
 		} 
  	}
  	for (auto i = entryToRemove.begin(); i != entryToRemove.end(); ++i) {
  		route_state.networkTable.erase(*i);
  	}

	time_t now = time(NULL);
	vector<string> toRemove;
  	// remove invalid neighbors in the network table.
  	// invalid neighbors are defined as the neighor HID must be in one of the dest HID of network table 
	for (auto i = route_state.networkTable.begin(); i != route_state.networkTable.end(); ++i) {
		toRemove.clear();
		// remove the neighbor for current destination that's not in network table.
		for (auto j = i->second.neighbor_hids.begin(); j != i->second.neighbor_hids.end(); ++j) {
			if (route_state.networkTable.find(*j) == route_state.networkTable.end()){
				toRemove.push_back(*j);
			}
		}
		for (auto j = toRemove.begin(); j != toRemove.end(); ++j) {
			i->second.neighbor_hids.erase(
				std::remove(i->second.neighbor_hids.begin(), i->second.neighbor_hids.end(), *j), i->second.neighbor_hids.end());
			i->second.num_neighbors--;
		}

		toRemove.clear();
		// remove the dest cids that's in the local routes
		for(auto j = i->second.dest_cids.begin(); j != i->second.dest_cids.end(); ++j){
			auto findInLocal = std::find(route_state.sourceCids.begin(), route_state.sourceCids.end(), j->first);

			if(findInLocal != route_state.sourceCids.end()){
				toRemove.push_back(j->first);
			}
		}
		for(auto j = toRemove.begin(); j != toRemove.end(); ++j){
			i->second.dest_cids.erase(*j);
		}

		toRemove.clear();
		// remove the expired dest cids in our local topology map
		for(auto j = i->second.dest_cids.begin(); j != i->second.dest_cids.end(); ++j){
			if(now - j->second >= EXPIRE_TIME){
				toRemove.push_back(j->first);
			}
		}

		for(auto j = toRemove.begin(); j != toRemove.end(); ++j){
			i->second.dest_cids.erase(*j);
		}
  	} 	
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
			
			if(sendLSA()) {
				syslog(LOG_ALERT, "ERROR: Failed sending LSA");
				exit(-1);
			}

			route_state.mtx.unlock();

            this_thread::sleep_for(chrono::milliseconds(nextTimeMilisecond));
        }
    }).detach();
}

void calcShortestPath() {
 	map<string, NodeStateEntry> table = route_state.networkTable;
  	for (auto it = route_state.networkTable.begin(); it != route_state.networkTable.end(); ++it) {
 		// initialize the checking variable
 		it->second.checked = false;
 		it->second.cost = 10000000;
  	}

	for (auto it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); ++it) {
		string neighbor = it->first;

		if(route_state.networkTable.find(neighbor) != route_state.networkTable.end()){
			route_state.networkTable[neighbor].cost = 1;
			route_state.networkTable[neighbor].prevNode = route_state.myHID;
		}
	}
	
	while (!table.empty()) {
		string selectedHID = "";
		int minCost = 10000000;
	
		for (auto it = table.begin(); it != table.end(); ++it) {
			if (route_state.networkTable[it->first].cost < minCost) {
				minCost = route_state.networkTable[it->first].cost;
				selectedHID = it->first;
			}
  		}

  		if(selectedHID.empty()){
  			return;
  		}

  		table.erase(selectedHID);
  		route_state.networkTable[selectedHID].checked = true;

 		for (auto it = route_state.networkTable[selectedHID].neighbor_hids.begin() ; it != route_state.networkTable[selectedHID].neighbor_hids.end(); ++it) {
			string selectedHIDNeighbor = *it;

			if (route_state.networkTable[selectedHIDNeighbor].checked == false) {
				if (route_state.networkTable[selectedHIDNeighbor].cost > route_state.networkTable[selectedHID].cost + 1) {
					route_state.networkTable[selectedHIDNeighbor].cost = route_state.networkTable[selectedHID].cost + 1;
					route_state.networkTable[selectedHIDNeighbor].prevNode = selectedHID;
				}
			}
		}
	}

	// clear the current CID route table
	route_state.CIDrouteTable.clear();
	// set up the nexthop for each appropriate CID
  	for (auto it = route_state.networkTable.begin(); it != route_state.networkTable.end(); ++it) {
  		// for this destination
  		string currDest = it->first;
  		string currNode = currDest;
  		
  		vector<string> path;
  		path.push_back(currNode);

  		int hop_count = 0;
  		while (route_state.networkTable[currNode].prevNode != route_state.myHID && hop_count < MAX_HOP_COUNT) {
  			currNode = route_state.networkTable[currNode].prevNode;
  			path.insert(path.begin(), currNode);
  			hop_count++;
  		}

  		if(hop_count < MAX_HOP_COUNT) {

#ifdef STATS_LOG
			logger->log("network table has destination HID: "+ currDest);
			string paths;
			for(auto it = path.begin(); it != path.end(); ++it){
				paths += *it + " ";
			}
			logger->log("path: " + paths);
#endif	

  			// for all the cids of the destination...
  			for (auto i = route_state.networkTable[currDest].dest_cids.begin(); i != route_state.networkTable[currDest].dest_cids.end(); ++i) {
  				string destCID = i->first;	

#ifdef STATS_LOG
  				logger->log("\ndest CID at that destination: " + destCID);
#endif

  				// if we haven't seen this CID routes before, set the routes
  				if(route_state.CIDrouteTable.find(destCID) == route_state.CIDrouteTable.end()){
  					route_state.CIDrouteTable[destCID].dest = currDest;
  					route_state.CIDrouteTable[destCID].nextHop = currNode;
  					route_state.CIDrouteTable[destCID].port = route_state.neighborTable[currNode].port;
  					route_state.CIDrouteTable[destCID].cost = hop_count + 1;
  				} else {
  					// if we have seen the CID routes before and cost is larger then, reset the routes
  					if(route_state.CIDrouteTable[destCID].cost > hop_count + 1){
  						route_state.CIDrouteTable[destCID].dest = currDest;
  						route_state.CIDrouteTable[destCID].nextHop = currNode;
  						route_state.CIDrouteTable[destCID].port = route_state.neighborTable[currNode].port;
  						route_state.CIDrouteTable[destCID].cost = hop_count + 1;
  					}
  				}
  			}
  		}
  	}
}

void removeOutdatedRoutes(const map<string, RouteEntry> & prevRoutes) {
  	for (auto it = prevRoutes.begin(); it != prevRoutes.end(); ++it) {
  		if(route_state.CIDrouteTable.find(it->first) == route_state.CIDrouteTable.end()){

#ifdef STATS_LOG
			logger->log("purging CID route " + it->first + " from " + it->second.dest);
#endif	
  			xr.delRouteCIDRouting(it->first);
  		}
  	}
}

void updateClickRoutingTable(const map<string, RouteEntry> & prevRoutes) {
  	int rc;
  	for (auto it =route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it) {

		if(prevRoutes.find(it->first) == prevRoutes.end()){		

#ifdef STATS_LOG
			logger->log("set CID route " + it->first + " from " + it->second.dest);
#endif	

			if ((rc = xr.setRouteCIDRouting(it->first, it->second.port, it->second.nextHop, 0xffff)) != 0){
				syslog(LOG_ERR, "error setting route %d", rc);
			}
		}
  	}
}

string composeLSA(vector<string> & cids, uint32_t seq){
	/* Message format (delimiter=^)
		ttl
		CID-msg-num
		LSA-seq-num
		source-HID
		num-source-cids
		source-CID1
		source-CID2
		...
		num-neighbors
		neighbor-HID1
		neighbor-HID2
		...
	*/
	std::ostringstream sstream;

	sstream << ttl << "^";
	sstream << seq << "^";
	sstream << route_state.lsa_seq << "^";
	sstream << route_state.myHID << "^";

	sstream << cids.size() << "^";
	for (std::vector<string>::iterator i = cids.begin(); i != cids.end(); ++i) {
		sstream << (*i) << "^";
	}

	sstream << route_state.neighborTable.size() << "^";
	for (map<std::string, NeighborEntry>::iterator i = route_state.neighborTable.begin(); i != route_state.neighborTable.end(); ++i) {
		std::string neighbor = i->first;
		sstream << neighbor << "^";
	}

	return sstream.str();
}

int sendLSAHelper(uint32_t & seq, int start, int end){
	int rc1 = 0, rc2 = 0, msglen, buflen;
	char buffer[MSG_CUTOFF];
	bzero(buffer, MSG_CUTOFF);

	vector<string> currCIDs;
	for(int i = start; i <= end; i++){
		currCIDs.push_back(route_state.sourceCids[i]);
	}

	string lsa = composeLSA(currCIDs, seq);
	msglen = lsa.size();

	if(msglen < MSG_CUTOFF){
		strcpy (buffer, lsa.c_str());
		buflen = strlen(buffer);

		sockaddr_x ddag;
		Graph g = Node() * Node(BHID) * Node(SID_XROUTE_RECV);
		g.fill_sockaddr(&ddag);

		rc1 = Xsendto(route_state.send_sock, buffer, buflen, 0, (struct sockaddr*)&ddag, sizeof(sockaddr_x));
		if(rc1 != buflen) {
			syslog(LOG_WARNING, "ERROR sending LSA. Tried sending %d bytes but rc=%d", buflen, rc1);
			return -1;
		}

		seq++;
	} else {
		rc1 = sendLSAHelper(seq, start, (start + end)/2);
		rc2 = sendLSAHelper(seq, (start + end)/2 + 1, end);
		
		if(rc1 < 0 || rc2 < 0){
			return -1;
		}
	}
		
	return rc1 + rc2;
}

// send the link state CID to other router
int sendLSA(){
	int rc = -1;
	uint32_t seq = 0;

	if(route_state.sourceCids.size() != 0){

#ifdef STATS_LOG
		logger->log("sending LSA");
		for(auto it = route_state.sourceCids.begin(); it != route_state.sourceCids.end(); ++it){
			logger->log("CID " + *it);
		}
#endif
		if ((rc = sendLSAHelper(seq, 0, route_state.sourceCids.size() - 1)) < 0){
			return -1;
		}
	}

	route_state.lsa_seq++;
	route_state.lsa_seq = route_state.lsa_seq % MAX_SEQNUM;

	return 0;
}

int processLSA(string lsa_msg) {
	size_t found, start;
	string msg, ttl_str, cid_num, lsa_seq, destHID, num_dest_cids, dest_cid, 
				num_neighbors, neighbor_hid, num_neighbor_cids, neighbor_cid;

	start = 0;
	msg = lsa_msg;

	found = msg.find("^", start);
  	if (found != string::npos) {
  		ttl_str = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

	int ttl = atoi(ttl_str.c_str());

	found = msg.find("^", start);
  	if (found != string::npos) {
  		cid_num = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	int cid_msg_num = atoi(cid_num.c_str());

	// read lsa seq number
	found = msg.find("^", start);
  	if (found != string::npos) {
  		lsa_seq = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	uint32_t lsa_seq_num = atoi(lsa_seq.c_str());

  	// read dest hid
  	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}	

  	// we don't want message from myself
  	if(destHID == route_state.myHID){
  		return 1;
  	}

  	// filter out the already seen LSA
	if(route_state.networkTable.find(destHID) != route_state.networkTable.end()) {
		// filter already seen LSA
  	  	if ((lsa_seq_num < route_state.networkTable[destHID].seq && route_state.networkTable[destHID].seq - lsa_seq_num < 1000000) || 
  	  		(lsa_seq_num == route_state.networkTable[destHID].seq && route_state.networkTable[destHID].cid_nums.find(cid_msg_num) != route_state.networkTable[destHID].cid_nums.end())) {
  			return 0;
  		}

  		if(lsa_seq_num > route_state.networkTable[destHID].seq){
			route_state.networkTable[destHID].cid_nums.clear();
  		}
  	}

#ifdef STATS_LOG
	logger->log("Receive valid LSA");
	logger->log("\t destHID " + destHID);
	logger->log("\t ttl " + ttl_str);
	logger->log("\t cid_num " + cid_num);
	logger->log("\t lsa_seq " + lsa_seq);
#endif

	route_state.networkTable[destHID].seq = lsa_seq_num;
	route_state.networkTable[destHID].cid_nums.insert(cid_msg_num);

  	// read num_dest_cids
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_dest_cids = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

#ifdef STATS_LOG
	logger->log("\t num_dest_cids " + num_dest_cids);
#endif

  	int dest_cids_num = atoi(num_dest_cids.c_str());
  	for (int i = 0; i < dest_cids_num; ++i) {
		// read CID
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			dest_cid = msg.substr(start, found-start);

#ifdef STATS_LOG
			logger->log("\t\tdest_cid " + dest_cid);
#endif		

			route_state.networkTable[destHID].dest_cids[dest_cid] = time(NULL);
  			start = found+1;  // forward the search point
  		}
  	}

  	// read num_neighbors
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_neighbors = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

#ifdef STATS_LOG
	logger->log("\t num_neighbors " + num_neighbors);
#endif	

  	int neighbors_num = atoi(num_neighbors.c_str());
  	route_state.networkTable[destHID].num_neighbors = neighbors_num;
  	route_state.networkTable[destHID].neighbor_hids.clear();

  	// read in neighbor hids
  	for (int i = 0; i < neighbors_num; ++i) {
  		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighbor_hid = msg.substr(start, found-start);
  			start = found+1;  // forward the search point

#ifdef STATS_LOG
			logger->log("\t\t neighbor_hid " + neighbor_hid);
#endif	

  			route_state.networkTable[destHID].neighbor_hids.push_back(neighbor_hid);
  		}
  	}

  	/* Core routing logic */
  	map<string, RouteEntry> CIDrouteTableOld = route_state.CIDrouteTable;
	removeAbnormalNetworkTable();
  	calcShortestPath();
  	removeOutdatedRoutes(CIDrouteTableOld);
  	updateClickRoutingTable(CIDrouteTableOld);

  	if(ttl - 1 > 0){

#ifdef STATS_LOG
		logger->log("have something to relay to neighbor");
#endif

  		char buffer[MSG_CUTOFF];
		bzero(buffer, MSG_CUTOFF);

  		found = msg.find("^", 0);
  		string relayMsg = msg.substr(found);
  		string newMsg = to_string(ttl - 1) + relayMsg;

  		// rebroadcast
  		strcpy(buffer, newMsg.c_str());
		if(Xsendto(route_state.send_sock, buffer, strlen(buffer), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x)) < 0){
			syslog(LOG_DEBUG, "problem with Xsendto");
			return -1;
		}
  	}

	return 1;
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

	// reading localhost address
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

	route_state.lsa_seq = 0;	// LSA sequence number of this router
}

void populateRouteState(std::vector<XIARouteEntry> & routeEntries){
	route_state.sourceCids.clear();

	for (std::vector<XIARouteEntry>::iterator i = routeEntries.begin(); i != routeEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(eachEntry.port == DESTINE_FOR_LOCALHOST){
			route_state.sourceCids.push_back(eachEntry.xid);
		}
	}
}

void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries){
	// first update the existing neighbor entry
	for (auto i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(!eachEntry.nextHop.empty()){
			if(route_state.neighborTable.find(eachEntry.nextHop) == route_state.neighborTable.end()) {
				NeighborEntry newNeighborEntry;
				newNeighborEntry.cost = 1;
				newNeighborEntry.port = eachEntry.port;
				route_state.neighborTable[eachEntry.nextHop] = newNeighborEntry;
			}
		}
	}

	// then remove those entries that have element in map, but not in the HID route table
	vector<std::string> entriesToRemove;
  	for (auto it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++) {
		string neighborHID = it->first;
		bool found = false;

		for (auto i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
			if(i->nextHop == neighborHID){
				found = true;
			}
		}

		if(!found){
			entriesToRemove.push_back(neighborHID);
		}
  	}

  	for (auto i = entriesToRemove.begin(); i != entriesToRemove.end(); ++i) {
  		route_state.neighborTable.erase(*i);
  	}
}

int main(int argc, char *argv[]){
	int rc, n, selectRetVal, iteration = 0;
	socklen_t dlen;
	sockaddr_x theirDAG;
	char recv_message[XIA_MAXBUF];
	fd_set socks;
	struct timeval timeoutval;

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
   	logger = new Logger(namePrefix + "_ls");
#endif

   	(void) signal(SIGINT, cleanup);

   	periodicJobs();

	// main event loop
	while(1){
		iteration++;
		FD_ZERO(&socks);
		FD_SET(route_state.recv_sock, &socks);

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000;

		selectRetVal = Xselect(route_state.recv_sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0){
			memset(recv_message, 0, sizeof(recv_message));

			printf("before recv from\n");
			n = Xrecvfrom(route_state.recv_sock, recv_message, XIA_MAXBUF, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("recvfrom got a problem");
			}

			printf("before processLSA\n");

			route_state.mtx.lock();

#if defined(STATS_LOG)
			logger->log("got a new LSA message\n");
#endif

			processLSA(recv_message);

			route_state.mtx.unlock();
		}
	}	
	
	return 0;
}