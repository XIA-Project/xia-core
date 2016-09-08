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

char *hostname = NULL;
char *ident = NULL;

XIARouter xr;
RouteState route_state;

void listRoutes(std::string xidType) {
	int rc;
	vector<XIARouteEntry> routes;
	syslog(LOG_INFO, "%s: route updates", xidType.c_str());
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			syslog(LOG_INFO, "%s: %s | %d | %s | %ld", xidType.c_str(), r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
		}
	} else if (rc == 0) {
		syslog(LOG_INFO, "%s: No routes exist", xidType.c_str());
	} else {
		syslog(LOG_WARNING, "%s: Error getting route list %d", xidType.c_str(), rc);
	}
	syslog(LOG_INFO, "%s: done listing route updates", xidType.c_str());
}


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

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
	printf(" -h hostname : click device name (default=router0)\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv)
{
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v")) != -1) {
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
			case '?':
			default:
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname){
		hostname = strdup(DEFAULT_NAME);
	}

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void cleanUpNetworkTable(){
	 // filter out an abnormal case
	vector<std::string> entryToRemove;
 	map<std::string, NodeStateEntry>::iterator it1;
  	for (it1 = route_state.networkTable.begin(); it1 != route_state.networkTable.end(); it1++ ) {
 		if(it1->second.num_neighbors == 0 || (it1->first).empty() || (it1->second.dest).empty() ) {
 			entryToRemove.push_back(it1->first);
 		} 
  	}
  	for (std::vector<std::string>::iterator i = entryToRemove.begin(); i != entryToRemove.end(); ++i) {
  		route_state.networkTable.erase(*i);
  	}

	for (it1 = route_state.networkTable.begin(); it1 != route_state.networkTable.end(); it1++ ) {
		vector<std::string> neighbors = it1->second.neighbor_hids;
		vector<std::string> toRemove;

		for (unsigned i = 0; i < neighbors.size(); i++) {
			map<std::string, NodeStateEntry>::iterator it = route_state.networkTable.find(neighbors[i]);
			if (it == route_state.networkTable.end()){
				toRemove.push_back(neighbors[i]);
			}
		}

		for (vector<std::string>::iterator i = toRemove.begin(); i != toRemove.end(); ++i) {
			it1->second.neighbor_hids.erase(
				std::remove(it1->second.neighbor_hids.begin(), it1->second.neighbor_hids.end(), *i), it1->second.neighbor_hids.end());
			it1->second.num_neighbors--;
		}
  	}
}

void calcShortestPath() {
	// first, clear the current routing table
	route_state.CIDrouteTable.clear();
	// clean up the network table (remove abnormal entries etc)
	cleanUpNetworkTable();
	
 	map<std::string, NodeStateEntry> table;
	table = route_state.networkTable;

	map<std::string, NodeStateEntry>::iterator it1;
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
 		// initialize the checking variable
 		it1->second.checked = false;
 		it1->second.cost = 10000000;
  	}

	// compute shortest path
	// initialization
	string myHID = route_state.myHID;
	route_state.networkTable[myHID].checked = true;
	route_state.networkTable[myHID].cost = 0;
	table.erase(myHID);

	vector<std::string>::iterator it2;
	for ( it2=route_state.networkTable[myHID].neighbor_hids.begin() ; it2 < route_state.networkTable[myHID].neighbor_hids.end(); it2++ ) {
		string tempHID = (*it2).c_str();
		route_state.networkTable[tempHID].cost = 1;
		route_state.networkTable[tempHID].prevNode = myHID;
	}

	while (!table.empty()) {
		int minCost = 10000000;
		string selectedHID;
		for ( it1=table.begin() ; it1 != table.end(); it1++ ) {
			string tmpHID = it1->second.dest;
			if (route_state.networkTable[tmpHID].cost < minCost) {
				minCost = route_state.networkTable[tmpHID].cost;
				selectedHID = tmpHID;
			}
  		}

  		//syslog(LOG_INFO, "selectedHID: %s", selectedHID.c_str());
		if(selectedHID.empty()) {
			return;
		}

  		table.erase(selectedHID);
  		route_state.networkTable[selectedHID].checked = true;

 		for (it2=route_state.networkTable[selectedHID].neighbor_hids.begin() ; it2 < route_state.networkTable[selectedHID].neighbor_hids.end(); it2++ ) {
			string tempHID = (*it2).c_str();
			if (route_state.networkTable[tempHID].checked != true) {
				if (route_state.networkTable[tempHID].cost > route_state.networkTable[selectedHID].cost + 1) {
					route_state.networkTable[tempHID].cost = route_state.networkTable[selectedHID].cost + 1;
					route_state.networkTable[tempHID].prevNode = selectedHID;
				}
			}
		}
	}

	string tempHID1, tempHID2;
	int hop_count;
	// set up the nexthop for each appropriate CID
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
  		// for this destination
  		tempHID1 = it1->second.dest;
  		// if it's not destined for myself
  		if ( myHID != tempHID1 ) {
  			tempHID2 = tempHID1;
  			hop_count = 0;
  			while (route_state.networkTable[tempHID2].prevNode != myHID && hop_count < MAX_HOP_COUNT) {
  				tempHID2 = route_state.networkTable[tempHID2].prevNode;
  				hop_count++;
  			}

  			if(hop_count < MAX_HOP_COUNT) {
  				vector<std::string> cidsForTempHID1 = route_state.networkTable[tempHID1].dest_cids;
  				// for all the cids of the destination...
  				for (std::vector<std::string>::iterator i = cidsForTempHID1.begin(); i != cidsForTempHID1.end(); ++i) {
  					bool found = false;
  					string destCID = *i;

  					for (std::vector<std::string>::iterator j = route_state.sourceCids.begin(); j != route_state.sourceCids.end(); ++j) {
  						string myCID = *j;
  						if(destCID == myCID){
  							found = true;
  						}
  					}

  					// if this cid is not in the current routing table, then need to set up the routing table
  					// otherwise, we don't insert because we already have the entry in the local table
  					if(!found){
  						route_state.CIDrouteTable[destCID].dest = destCID;
  						route_state.CIDrouteTable[destCID].nextHop = route_state.neighborTable[tempHID2].HID;
  						route_state.CIDrouteTable[destCID].port = route_state.neighborTable[tempHID2].port;
  					}
  				}
  			}
  		}
  	}
}

void printRoutingTable(){
	syslog(LOG_INFO, "CID Routing table at %s", route_state.myHID);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.CIDrouteTable.begin() ; it1 != route_state.CIDrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags));
  	}
}

void updateClickRoutingTable() {
	int rc, port;
	string destXID, nexthopXID;

  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.CIDrouteTable.begin(); it1 != route_state.CIDrouteTable.end(); it1++ ) {
 		destXID = it1->second.dest;
 		nexthopXID = it1->second.nextHop;
		port =  it1->second.port;

		if ((rc = xr.setRouteCIDRouting(destXID, port, nexthopXID, 0xffff)) != 0){
			syslog(LOG_ERR, "error setting route %d", rc);
		}
  	}
}

string composeLSA(vector<string> & cids, int seq){
	/* Message format (delimiter=^)
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

int sendLSAHelper(int & seq, int start, int end){
	int rc1 = 0, rc2 = 0, msglen, buflen;
	char buffer[XIA_MAXBUF];
	bzero(buffer, XIA_MAXBUF);

	vector<string> temp;
	for(int i = start; i <= end; i++){
		temp.push_back(route_state.sourceCids[i]);
	}

	string lsa = composeLSA(temp, seq);
	msglen = lsa.size();

	// for now lets assume that topology is small, and message is only oversized if
	// because of CIDs
	if(msglen < XIA_MAXBUF){
		syslog(LOG_INFO, "sending LSA of seq %d start %d end %d\n", seq, start, end);

		strcpy (buffer, lsa.c_str());
		buflen = strlen(buffer);

		rc1 = Xsendto(route_state.sock, buffer, buflen, 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));
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
	int rc = -1, seq = 0;
	if ((rc = sendLSAHelper(seq, 0, route_state.sourceCids.size() - 1)) < 0){
		return -1;
	}

	route_state.lsa_seq++;
	route_state.lsa_seq = route_state.lsa_seq % MAX_SEQNUM;

	return 0;
}

void initRouteState() {
	// make the dest DAG (broadcast to other routers)
	Graph g = Node() * Node(BHID) * Node(SID_XROUTE);
	g.fill_sockaddr(&route_state.ddag);

	syslog(LOG_INFO, "xroute Broadcast DAG: %s", g.dag_string().c_str());

	char cdag[MAX_DAG_SIZE];
	// read the localhost AD and HID
    if (XreadLocalHostAddr(route_state.sock, cdag, MAX_DAG_SIZE, route_state.my4ID, MAX_XID_SIZE) < 0 ){
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}

    Graph g_localhost(cdag);
	strcpy(route_state.myAD, g_localhost.intent_AD_str().c_str());
    strcpy(route_state.myHID, g_localhost.intent_HID_str().c_str());


	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.sdag, ai->ai_addr, sizeof(sockaddr_x));

	// bind to the src DAG
   	if (Xbind(route_state.sock, (struct sockaddr*)&route_state.sdag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&route_state.sdag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(route_state.sock);
   		exit(-1);
   	}

	route_state.lsa_seq = 0;	// LSA sequence number of this router
	route_state.calc_dijstra_ticks = 0;
}

void populateRouteState(std::vector<XIARouteEntry> & routeEntries){
	// clean up old stuff, only change the source cid route state for now
	// assume that routes to others aren't touched
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
	for (std::vector<XIARouteEntry>::iterator i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(eachEntry.nextHop != ""){
			map<std::string, NeighborEntry>::iterator it;
			it = route_state.neighborTable.find(eachEntry.nextHop);
			if(it == route_state.neighborTable.end()) {
				NeighborEntry newNeighborEntry;
				newNeighborEntry.cost = 1;
				newNeighborEntry.port = eachEntry.port;
				newNeighborEntry.HID = eachEntry.nextHop;

				route_state.neighborTable[eachEntry.nextHop] = newNeighborEntry;
			} else {
				route_state.neighborTable[eachEntry.nextHop].cost = 1;
				route_state.neighborTable[eachEntry.nextHop].port = eachEntry.port;
				route_state.neighborTable[eachEntry.nextHop].HID = eachEntry.nextHop;
			}
		}
	}

	// then remove those entries that have element in map, but not in the HID route table
	vector<std::string> entriesToRemove;
	map<std::string, NeighborEntry>::iterator it;
  	for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++ ) {
		bool found = false;
		std::string checkXid = it->first;

		for (std::vector<XIARouteEntry>::iterator i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
			if((*i).xid == checkXid){
				found = true;
			}
		}

		if(!found){
			entriesToRemove.push_back(checkXid);
		}
  	}

  	for (std::vector<std::string>::iterator i = entriesToRemove.begin(); i != entriesToRemove.end(); ++i) {
  		route_state.neighborTable.erase(*i);
  	}
}

int processLSA(std::string lsa_msg) {
	char buffer[XIA_MAXBUF];
	bzero(buffer, XIA_MAXBUF);

	size_t found, start;
	string msg, cid_num, lsa_seq, destHID, num_dest_cids, dest_cid, num_neighbors, neighbor_hid, num_neighbor_cids, neighbor_cid;

	start = 0;
	msg = lsa_msg;

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

  	int lsa_seq_num = atoi(lsa_seq.c_str());

  	// read dest hid
  	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	// we don't want message from myself
  	string myHID = route_state.myHID;
  	if(destHID == myHID){
  		return 1;
  	}

  	// filter out the already seen LSA
	map<std::string, NodeStateEntry>::iterator it;
	set<int> prevCIDNums;
	set<int>::iterator it2;
	vector<std::string> prevCIDs;
	it=route_state.networkTable.find(destHID);

	if(it != route_state.networkTable.end()) {
		it2 = it->second.cid_nums.find(cid_msg_num);

  	  	if ((lsa_seq_num < it->second.seq && it->second.seq - lsa_seq_num < 10000) || 
  	  						(lsa_seq_num == it->second.seq && it2 != it->second.cid_nums.end())) {
  	  		// If this LSA already seen, ignore this LSA; do nothing
  			return 1;
  		}

  		// save the state from the previous round
  		// provided they are in the same round
  		if(lsa_seq_num == it->second.seq){
  			prevCIDNums = it->second.cid_nums;
  			for (unsigned i = 0; i < it->second.dest_cids.size(); ++i) {
  				prevCIDs.push_back(it->second.dest_cids[i]);
  			}
  		}
  		
  		// For now, delete this dest entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase(it);
  	}

  	// create new node state entry
  	NodeStateEntry entry;
  	entry.dest = destHID;
	entry.seq = lsa_seq_num;
	entry.cid_nums = prevCIDNums;
	entry.cid_nums.insert(cid_msg_num);

  	// read num_dest_cids
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_dest_cids = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int dest_cids_num = atoi(num_dest_cids.c_str());

  	for (unsigned i = 0; i < prevCIDs.size(); ++i) {
  		entry.dest_cids.push_back(prevCIDs[i]);
  	}

  	for (int i = 0; i < dest_cids_num; ++i) {
		// read CID
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			dest_cid = msg.substr(start, found-start);
  			entry.dest_cids.push_back(dest_cid);
  			start = found+1;  // forward the search point
  		}
  	}

  	// read num_neighbors
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_neighbors = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int neighbors_num = atoi(num_neighbors.c_str());
  	entry.num_neighbors = neighbors_num;

  	// read in neighbor hids
  	for (int i = 0; i < neighbors_num; ++i) {
  		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighbor_hid = msg.substr(start, found-start);
  			start = found+1;  // forward the search point

  			entry.neighbor_hids.push_back(neighbor_hid);
  		}
  	}

  	route_state.networkTable[destHID] = entry;

  	// for the shortest path and update click routing table
  	route_state.calc_dijstra_ticks++;

  	if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
  		calcShortestPath();
  		updateClickRoutingTable();
  		route_state.calc_dijstra_ticks = 0;
	}

  	// rebroadcast
  	strcpy(buffer, msg.c_str());
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));

	return -1;
}

void printNetworkTable(){
	syslog(LOG_NOTICE, "******************************* The network table is: *******************************\n");
	map<std::string, NodeStateEntry>::iterator it;
	set<int>::iterator it2;
  	for (it = route_state.networkTable.begin(); it != route_state.networkTable.end(); it++ ) {
  		NodeStateEntry entry = it->second;
  		syslog(LOG_NOTICE, "destination hid key: %s", it->first.c_str());
  		syslog(LOG_NOTICE, "destination hid entry: %s", entry.dest.c_str());
  		syslog(LOG_NOTICE, "seq number entry: %d", entry.seq);
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

void printNeighborTable(){
	syslog(LOG_NOTICE, "******************************* The neighbor table is: *******************************\n");
	map<std::string, NeighborEntry>::iterator it;
  	for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++ ) {
  		std::string neighborHID = it->first;
  		NeighborEntry entry = it->second;
  		syslog(LOG_NOTICE, "HID: %s", neighborHID.c_str());
  		syslog(LOG_NOTICE, "entry cost: %d", entry.cost);
  		syslog(LOG_NOTICE, "entry port: %d", entry.port);
  		syslog(LOG_NOTICE, "entry HID: %s", entry.HID.c_str());
   	}
}

void fillMeToMyNetworkTable(){
	// fill my neighbors into my entry in the networkTable
  	NodeStateEntry entry;
	entry.dest = route_state.myHID;
	entry.seq = route_state.lsa_seq;
	entry.num_neighbors = route_state.neighborTable.size();
	entry.dest_cids = route_state.sourceCids;

  	map<std::string, NeighborEntry>::iterator it3;
  	for ( it3=route_state.neighborTable.begin() ; it3 != route_state.neighborTable.end(); it3++ ) {
 		entry.neighbor_hids.push_back(it3->second.HID);
  	}

  	route_state.networkTable[route_state.myHID] = entry;
}

int main(int argc, char *argv[])
{
	int rc, n, selectRetVal, iteration = 0;
	socklen_t dlen;
	sockaddr_x theirDAG;
	char recv_message[XIA_MAXBUF];
	fd_set socks;
	struct timeval timeoutval;
	std::vector<XIARouteEntry> currHidRouteEntries, currCidRouteEntries;

	dlen = sizeof(sockaddr_x);

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

    // connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		return -1;
	}

	xr.setRouter(hostname);

	// open socket for route process
   	route_state.sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
   	if (route_state.sock < 0) {
   		syslog(LOG_ALERT, "Unable to create a socket");
   		exit(-1);
   	}

   	initRouteState();

	// main event loop
	while(1){
		iteration++;
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = MAIN_LOOP_USEC * 2; // Main loop runs every 1000 usec

		selectRetVal = Xselect(route_state.sock + 1, &socks, NULL, NULL, &timeoutval);

		if (selectRetVal > 0){
			memset(&recv_message[0], 0, sizeof(recv_message));

			n = Xrecvfrom(route_state.sock, recv_message, XIA_MAXBUF, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("recvfrom got a problem");
			}

			processLSA(recv_message);
		}
		// to get neighbor information every 1 sec
		if(iteration % CHECK_NEIGHBOR_ENTRY_ITERS == 0){
			getRouteEntries("HID", currHidRouteEntries);
			populateNeighborState(currHidRouteEntries);
			fillMeToMyNetworkTable();
		}

		// populate the route state for cids every 2 sec
		if(iteration % CHECK_CID_ENTRY_ITERS == 0){
			getRouteEntries("CID", currCidRouteEntries);
			populateRouteState(currCidRouteEntries);
			fillMeToMyNetworkTable();

			if(sendLSA()) {
				syslog(LOG_WARNING, "ERROR: Failed sending LSA");
			}
		}
	}	
	
	return 0;
}