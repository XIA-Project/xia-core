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

char *hostname = NULL;
char *ident = NULL;

XIARouter xr;
RouteState route_state;

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-t] [-h hostname]\n", name);
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
				// Help Me!
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

void listRoutes(std::string xidType)
{
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

void printRoutingTable(){
	syslog(LOG_INFO, "CID Routing table at %s", route_state.myHID);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.CIDrouteTable.begin() ; it1 != route_state.CIDrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port));
  	}
}

void initRouteState() {
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
}

void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries){
	// first update the existing neighbor entry
	for (std::vector<XIARouteEntry>::iterator i = currHidRouteEntries.begin(); i != currHidRouteEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		set<std::string>::iterator it;
		it = route_state.neighbors.find(eachEntry.nextHop);
		if(it == route_state.neighbors.end()) {
			route_state.neighbors.insert(eachEntry.nextHop);
		}
	}

	// then remove those entries that have element in map, but not in the HID route table
	vector<std::string> entriesToRemove;
	set<std::string>::iterator it;
  	for (it = route_state.neighbors.begin(); it != route_state.neighbors.end(); it++ ) {
		bool found = false;
		std::string checkXid = *it;

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
  		route_state.neighbors.erase(*i);
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

void updateClickRoutingTable() {
	int rc, port;
	string destXID, nexthopXID;

  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.CIDrouteTable.begin(); it1 != route_state.CIDrouteTable.end(); it1++ ) {
 		destXID = it1->second.dest;
 		nexthopXID = it1->second.nextHop;
		port =  it1->second.port;

		if ((rc = xr.setRoute(destXID, port, nexthopXID, 0xffff)) != 0){
			syslog(LOG_ERR, "error setting route %d", rc);
		}
  	}
}

int processRIPUpdate(string rip_msg) {
	size_t found, start;
	string msg, source_hid, cid_num_str, cid, cost;

	start = 0;
	msg = rip_msg;

	found = msg.find("^", start);
  	if (found != string::npos) {
  		source_hid = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

  	int hid_interface = interfaceNumber("HID", source_hid);

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

  		int curr_cost = atoi(cost.c_str());

  		map<std::string, RouteEntry>::iterator it;
  		it = route_state.CIDrouteTable.find(cid);

  		if (it != route_state.CIDrouteTable.end()){
  			if(route_state.CIDrouteTable[cid].cost > curr_cost + 1){
  				route_state.CIDrouteTable[cid].cost = curr_cost + 1;
  				route_state.CIDrouteTable[cid].port = hid_interface;
  				route_state.CIDrouteTable[cid].dest = cid;
  				route_state.CIDrouteTable[cid].nextHop = source_hid;
  				change = true;
  			}
  		} else {
  			route_state.CIDrouteTable[cid].dest = cid;
  			route_state.CIDrouteTable[cid].port = hid_interface;
  			route_state.CIDrouteTable[cid].nextHop = source_hid;
  			route_state.CIDrouteTable[cid].cost = curr_cost + 1;
  			change = true;
  		}
  	}

  	if(change){
  		updateClickRoutingTable();
   	}

	return 1;
}

/* Message format (delimiter=^)
	source-HID
	num-source-cids
	source-CID1
	distance-cid1
	source-CID2
	distance-cid2
	...
*/
string constructBroadcastRIP(vector<string> & cids){
	std::ostringstream sstream;

	sstream << route_state.myHID << "^";
	sstream << cids.size() << "^";
	for (std::vector<string>::iterator i = cids.begin(); i != cids.end(); ++i) {
		RouteEntry curr = route_state.CIDrouteTable[*i];

		sstream << (*i) << "^";
		sstream << curr.cost << "^";
	}

	return sstream.str();
}


int sendBroadcastRIPHelper(string neighborHID, int start, int end){
	int rc1 = 0, rc2 = 0, msglen, buflen;
	char buffer[XIA_MAXBUF];
	bzero(buffer, XIA_MAXBUF);

	vector<string> temp;
	for(int i = start; i <= end; i++){
		temp.push_back(route_state.sourceCids[i]);
	}

	string rip = constructBroadcastRIP(temp);
	msglen = rip.size();

	if(msglen < XIA_MAXBUF){
		strcpy (buffer, rip.c_str());
		buflen = strlen(buffer);

		sockaddr_x ddag;
		Graph g = Node() * Node(neighborHID) * Node(SID_XROUTE);
		g.fill_sockaddr(&ddag);

		rc1 = Xsendto(route_state.sock, buffer, buflen, 0, (struct sockaddr*)&ddag, sizeof(sockaddr_x));
		if(rc1 != buflen) {
			syslog(LOG_WARNING, "ERROR sending LSA. Tried sending %d bytes but rc=%d", buflen, rc1);
			return -1;
		}
	} else {
		rc1 = sendBroadcastRIPHelper(neighborHID, start, (start + end)/2);
		rc2 = sendBroadcastRIPHelper(neighborHID, (start + end)/2 + 1, end);
		
		if(rc1 < 0 || rc2 < 0){
			return -1;
		}
	}

	return rc1 + rc2;
}

int sendBroadcastRIP(string neighborHID){
	int rc = -1;
	if ((rc = sendBroadcastRIPHelper(neighborHID, 0, route_state.sourceCids.size() - 1)) < 0){
		return -1;
	}

	return rc;
}


// send RIP
void broadcastRIP() {
	std::string neighborHID;
	set<std::string>::iterator it;
	
	// send my routing table to my neighbor
	for (it = route_state.neighbors.begin(); it != route_state.neighbors.end(); it++) {
		neighborHID = *it;

		if(sendBroadcastRIP(neighborHID) < 0){
			syslog(LOG_WARNING, "cannot broad cast to neighborHID: %s\n", neighborHID.c_str());
		}
	}
}

void populateRouteState(std::vector<XIARouteEntry> & routeEntries){
	// clean up old stuff, only change the source cid route state for now
	// assume that routes to others aren't touched
	route_state.sourceCids.clear();

	map<std::string, RouteEntry>::iterator it;
	for (std::vector<XIARouteEntry>::iterator i = routeEntries.begin(); i != routeEntries.end(); ++i) {
		XIARouteEntry eachEntry = *i;

		if(eachEntry.port == DESTINE_FOR_LOCALHOST){
			route_state.sourceCids.push_back(eachEntry.xid);
		}
	}

	vector<std::string> entriesToRemove;
	for(it = route_state.CIDrouteTable.begin(); it != route_state.CIDrouteTable.end(); ++it){
		bool found = false;
		std::string checkXid = it->first;

		for(auto i = route_state.sourceCids.begin(); i !=  route_state.sourceCids.end(); i++){
			if(checkXid == *i){
				found = true;
			}
		}

		if(found){
			entriesToRemove.push_back(it->first);
		}
	}

	for (std::vector<std::string>::iterator i = entriesToRemove.begin(); i != entriesToRemove.end(); ++i) {
  		route_state.CIDrouteTable.erase(*i);
  	}
}

int main(int argc, char *argv[]) {
	int rc, selectRetVal, n, iteration = 0;
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
   	route_state.sock=Xsocket(AF_XIA, SOCK_DGRAM, 0);
   	if (route_state.sock < 0) {
   		syslog(LOG_ALERT, "Unable to create a socket");
   		exit(-1);
   	}

   	initRouteState();

	while (1) {
		iteration++;
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);

		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = MAIN_LOOP_USEC * 2; // Main loop runs every 2000 usec

		// every 0.002 sec, check if any received packets
		selectRetVal = Xselect(route_state.sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			memset(recv_message, 0, sizeof(recv_message));
			
			n = Xrecvfrom(route_state.sock, recv_message, XIA_MAXBUF, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("Xrecvfrom have a problem");
			}
			
			if(processRIPUpdate(recv_message) < 0){
				syslog(LOG_WARNING, "problem with processing RIP update\n");
			}
		}

		// check neighbor entry every second
		if(iteration % CHECK_NEIGHBOR_ENTRY_ITERS == 0){
			getRouteEntries("HID", currHidRouteEntries);
			populateNeighborState(currHidRouteEntries);
		}

		// check broadcast RIP every 2 seconds
		if(iteration % BROADCAST_RIP_ITERS == 0){
			getRouteEntries("CID", currCidRouteEntries);
			populateRouteState(currCidRouteEntries);
			broadcastRIP();
		}
    }

	return 0;
}