#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <map>
#include <time.h>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include "xrouted.hh"
#include "dagaddr.hpp"

#define DEFAULT_NAME "router0"
#define APPNAME "xrouted"
#define EXPIRE_TIME 10

char *hostname = NULL;
char *ident = NULL;

RouteState route_state;

XIARouter xr;
map<string,time_t> timeStamp;

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


void timeout_handler(int signum)
{
	UNUSED(signum);

	if (route_state.hello_seq < route_state.hello_lsa_ratio) {
		// send Hello
		sendHello();
		route_state.hello_seq++;
	} else if (route_state.hello_seq == route_state.hello_lsa_ratio) {
		// it's time to send LSA
		sendLSA();
		// reset hello req
		route_state.hello_seq = 0;
	} else {
		syslog(LOG_ERR, "hello_seq=%d hello_lsa_ratio=%d", route_state.hello_seq, route_state.hello_lsa_ratio);
	}
	// reset the timer
	signal(SIGALRM, timeout_handler);
	ualarm((int)ceil(HELLO_INTERVAL*1000000),0);
}

// send Hello message (1-hop broadcast)
int sendHello(){
	// Send my AD and my HID to the directly connected neighbors
	char buffer[1024];
	bzero(buffer, 1024);

	/* Message format (delimiter=^)
		message-type{Hello=0 or LSA=1}
		source-AD
		source-HID
	*/
	string hello;
	hello.append("0^");
	hello.append(route_state.myAD);
	hello.append("^");
	hello.append(route_state.myHID);
	hello.append("^");
	strcpy (buffer, hello.c_str());
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));
	return 1;
}

// send LinkStateAdvertisement message (flooding)
int sendLSA() {
	char buffer[1024];
	bzero(buffer, 1024);
	/* Message format (delimiter=^)
		message-type{Hello=0 or LSA=1}
		router-type{XIA=0 or XIA-IPv4-Dual=1}
		source-AD
		source-HID
		LSA-seq-num
		num_neighbors
		neighbor1-AD
		neighbor1-HID
		neighbor2-AD
		neighbor2-HID
		...
	*/
	string lsa;
	char lsa_seq[10], num_neighbors[10], is_dual_router[10];

	sprintf(lsa_seq, "%d", route_state.lsa_seq);
	sprintf(num_neighbors, "%d", route_state.num_neighbors);
	sprintf(is_dual_router, "%d", route_state.dual_router);

	lsa.append("1^");
	lsa.append(is_dual_router);
	lsa.append("^");
	lsa.append(route_state.myAD);
	lsa.append("^");
	lsa.append(route_state.myHID);
	lsa.append("^");
	lsa.append(lsa_seq);
	lsa.append("^");
	lsa.append(num_neighbors);
	lsa.append("^");

	map<std::string, NeighborEntry>::iterator it;

  	for ( it=route_state.neighborTable.begin() ; it != route_state.neighborTable.end(); it++ ) {
		lsa.append( it->second.AD );
		lsa.append("^");
		lsa.append( it->second.HID );
		lsa.append("^");
  	}
	strcpy (buffer, lsa.c_str());
	// increase the LSA seq
	route_state.lsa_seq++;
	route_state.lsa_seq = route_state.lsa_seq % MAX_SEQNUM;
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));
	return 1;
}

// process a Host Register message
void processHostRegister(const char* host_register_msg) {
	/* Procedure:
		1. update this host entry in (click-side) HID table:
			(hostHID, interface#, hostHID, -)
	*/
	int rc;
	size_t found, start;
	string msg, hostHID;
	start = 0;
	msg = host_register_msg;
 	// read message-type
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		start = found+1;   // message-type was previously read
  	}

	// read hostHID
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		hostHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

 	int interface = interfaceNumber("HID", hostHID);
	// update the host entry in (click-side) HID table
	if ((rc = xr.setRoute(hostHID, interface, hostHID, 0xffff)) != 0)
		syslog(LOG_ERR, "unable to set route %d", rc);

	timeStamp[hostHID] = time(NULL);
}

// process an incoming Hello message
int processHello(const char* hello_msg) {
	/* Procedure:
		1. fill in the neighbor table
		2. update my entry in the networkTable
	*/
	// 1. fill in the neighbor table
	size_t found, start;
	string msg, neighborAD, neighborHID, myAD;

	start = 0;
	msg = hello_msg;

 	// read message-type
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		start = found+1;   // message-type was previously read
  	}

	// read neighborAD
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		neighborAD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

 	// read neighborHID
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		neighborHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

	// fill in the table
	map<std::string, NeighborEntry>::iterator it;
	it=route_state.neighborTable.find(neighborAD);
	if(it == route_state.neighborTable.end()) {
		// if no entry yet
		NeighborEntry entry;
		entry.AD = neighborAD;
		entry.HID = neighborHID;
		entry.cost = 1; // for now, same cost

		int interface = interfaceNumber("HID", neighborHID);
		entry.port = interface;

		route_state.neighborTable[neighborAD] = entry;

		// increase the neighbor count
		route_state.num_neighbors++;
	}

	// 2. update my entry in the networkTable
	myAD = route_state.myAD;

	map<std::string, NodeStateEntry>::iterator it2;
	it2=route_state.networkTable.find(myAD);

	if(it2 != route_state.networkTable.end()) {

  		// For now, delete my entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase (it2);
  	}

	NodeStateEntry entry;
	entry.dest = myAD;
	entry.seq = route_state.lsa_seq;
	entry.num_neighbors = route_state.num_neighbors;

  	map<std::string, NeighborEntry>::iterator it3;
  	for ( it3=route_state.neighborTable.begin() ; it3 != route_state.neighborTable.end(); it3++ ) {

 		// fill my neighbors into my entry in the networkTable
 		entry.neighbor_list.push_back(it3->second.AD);
  	}

	route_state.networkTable[myAD] = entry;

	return 1;
}

// process a LinkStateAdvertisement message
int processLSA(const char* lsa_msg) {

	char buffer[1024];
	bzero(buffer, 1024);
	/* Procedure:
		0. scan this LSA (mark AD with a DualRouter if there)
		1. filter out the already seen LSA (via LSA-seq for this dest)
		2. update the network table
		3. rebroadcast this LSA
	*/
	// 0. Read this LSA
	size_t found, start;
	string msg, routerType, destAD, destHID, lsa_seq, num_neighbors, neighborAD, neighborHID;

	start = 0;
	msg = lsa_msg;

 	// read message-type
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		start = found+1;   // message-type was previously read
  	}

	// read routerType
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		routerType = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int is_dual_router = atoi(routerType.c_str());

	// read destAD
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destAD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

 	// read destHID
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}

	// read LSA-seq-num
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		lsa_seq = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int lsaSeq = atoi(lsa_seq.c_str());

 	// read num_neighbors
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_neighbors = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int numNeighbors = atoi(num_neighbors.c_str());

  	// See if this LSA comes from AD with dualRouter
  	if (is_dual_router == 1) {
  		route_state.dual_router_AD = destAD;
  	}

  	// First, filter out the LSA originating from myself
  	string myAD = route_state.myAD;
  	if (myAD.compare(destAD) == 0) {
  		return 1;
  	}

  	// 1. Filter out the already seen LSA
	map<std::string, NodeStateEntry>::iterator it;
	it=route_state.networkTable.find(destAD);

	if(it != route_state.networkTable.end()) {
  		// If this originating AD has been known (i.e., already in the networkTable)

  	  	if (lsaSeq <= it->second.seq  &&  it->second.seq - lsaSeq < 10000) {
  	  		// If this LSA already seen, ignore this LSA; do nothing
  			return 1;
  		}
  		// For now, delete this dest AD entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase (it);
  	}

	// 2. Update the network table
	NodeStateEntry entry;
	entry.dest = destAD;
	entry.seq = lsaSeq;
	entry.num_neighbors = numNeighbors;

  	int i;
 	for (i = 0; i < numNeighbors; i++) {

 		// read neighborAD
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighborAD = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

 		// read neighborHID
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighborHID = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}

 		// fill the neighbors into the corresponding networkTable entry
 		entry.neighbor_list.push_back(neighborAD);

 	}

	route_state.networkTable[destAD] = entry;
	//printf("LSA received: src=%s, seq=%d, num_neighbors=%d \n", (route_state.networkTable[destAD].dest).c_str(), route_state.networkTable[destAD].seq, route_state.networkTable[destAD].num_neighbors );
	route_state.calc_dijstra_ticks++;

	if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
		// Calculate Shortest Path algorithm
		syslog(LOG_INFO, "Calcuating shortest paths\n");
		calcShortestPath();
		route_state.calc_dijstra_ticks = 0;

		// update Routing table (click routing table as well)
		updateClickRoutingTable();
	}

	// 5. rebroadcast this LSA
	strcpy (buffer, lsa_msg);
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));

	return 1;
}


void calcShortestPath() {

	// first, clear the current routing table
	route_state.ADrouteTable.clear();

 	map<std::string, NodeStateEntry>::iterator it1;
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {

 		// filter out an abnormal case
 		if(it1->second.num_neighbors == 0 || (it1->second.dest).empty() ) {
 			route_state.networkTable.erase (it1);
 		}
  	}

 	map<std::string, NodeStateEntry> table;
	table = route_state.networkTable;

  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
 		// initialize the checking variable
 		it1->second.checked = false;
 		it1->second.cost = 10000000;
  	}

	// compute shortest path
	// initialization
	string myAD, tempAD;
	myAD = route_state.myAD;
	route_state.networkTable[myAD].checked = true;
	route_state.networkTable[myAD].cost = 0;
	table.erase(myAD);

	vector<std::string>::iterator it2;
	for ( it2=route_state.networkTable[myAD].neighbor_list.begin() ; it2 < route_state.networkTable[myAD].neighbor_list.end(); it2++ ) {

		tempAD = (*it2).c_str();
		route_state.networkTable[tempAD].cost = 1;
		route_state.networkTable[tempAD].prevNode = myAD;
	}

	// loop
	while (!table.empty()) {
		int minCost = 10000000;
		string selectedAD, tmpAD;
		for ( it1=table.begin() ; it1 != table.end(); it1++ ) {
			tmpAD = it1->second.dest;
			if (route_state.networkTable[tmpAD].cost < minCost) {
				minCost = route_state.networkTable[tmpAD].cost;
				selectedAD = tmpAD;
			}
  		}
		if(selectedAD.empty()) {
			return;
		}

  		table.erase(selectedAD);
  		route_state.networkTable[selectedAD].checked = true;

 		for ( it2=route_state.networkTable[selectedAD].neighbor_list.begin() ; it2 < route_state.networkTable[selectedAD].neighbor_list.end(); it2++ ) {
			tempAD = (*it2).c_str();
			if (route_state.networkTable[tempAD].checked != true) {
				if (route_state.networkTable[tempAD].cost > route_state.networkTable[selectedAD].cost + 1) {
					route_state.networkTable[tempAD].cost = route_state.networkTable[selectedAD].cost + 1;
					route_state.networkTable[tempAD].prevNode = selectedAD;
				}
			}
		}
	}

	string tempAD1, tempAD2;
	int hop_count;
	// set up the nexthop
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {

  		tempAD1 = it1->second.dest;
  		if ( myAD.compare(tempAD1) != 0 ) {
  			tempAD2 = tempAD1;
  			hop_count = 0;
  			while (route_state.networkTable[tempAD2].prevNode.compare(myAD)!=0 && hop_count < MAX_HOP_COUNT) {
  				tempAD2 = route_state.networkTable[tempAD2].prevNode;
  				hop_count++;
  			}
  			if(hop_count < MAX_HOP_COUNT) {
  				route_state.ADrouteTable[tempAD1].dest = tempAD1;
  				route_state.ADrouteTable[tempAD1].nextHop = route_state.neighborTable[tempAD2].HID;
  				route_state.ADrouteTable[tempAD1].port = route_state.neighborTable[tempAD2].port;
  			}
  		}
  	}
	printRoutingTable();
}


void printRoutingTable() {

	syslog(LOG_INFO, "AD Routing table at %s", route_state.myAD);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );

  	}
}


void updateClickRoutingTable() {

	int rc, port;
	string destXID, nexthopXID;
	string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-");

  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
 		destXID = it1->second.dest;
 		nexthopXID = it1->second.nextHop;
		port =  it1->second.port;

		if ((rc = xr.setRoute(destXID, port, nexthopXID, 0xffff)) != 0)
			syslog(LOG_ERR, "error setting route %d", rc);

		// set default AD for 4ID traffic
		if (route_state.dual_router==0 && destXID.compare(route_state.dual_router_AD)==0) {
			if ((rc = xr.setRoute(default_4ID, port, nexthopXID, 0xffff)) != 0)
				syslog(LOG_ERR, "error setting route %d", rc);
		}
  	}
  	listRoutes("AD");
	listRoutes("HID");
}



void initRouteState()
{
	// make the dest DAG (broadcast to other routers)
	Graph g = Node() * Node(BHID) * Node(SID_XROUTE);
	g.fill_sockaddr(&route_state.ddag);

	syslog(LOG_INFO, "xroute Broadcast DAG: %s", g.dag_string().c_str());

	// read the localhost AD and HID
	if ( XreadLocalHostAddr(route_state.sock, route_state.myAD, MAX_XID_SIZE, route_state.myHID, MAX_XID_SIZE, route_state.my4ID, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.sdag, ai->ai_addr, sizeof(sockaddr_x));

	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.lsa_seq = 0;	// LSA sequence number of this router
	route_state.hello_seq = 0;  // hello seq number of this router
	route_state.hello_lsa_ratio = (int32_t) ceil(LSA_INTERVAL/HELLO_INTERVAL);
	route_state.calc_dijstra_ticks = 0;

	route_state.dual_router_AD = "NULL";
	// mark if this is a dual XIA-IPv4 router
	if( XisDualStackRouter(route_state.sock) == 1 ) {
		route_state.dual_router = 1;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	} else {
		route_state.dual_router = 0;
	}

	// set timer for HELLO/LSA
	signal(SIGALRM, timeout_handler);
	ualarm((int)ceil(HELLO_INTERVAL*1000000),0); 	
}

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)");
	printf(" -v          : log to the console as well as syslog");
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

	if (!hostname)
		hostname = strdup(DEFAULT_NAME);

	// load the config setting for this hostname
	set_conf("xsockconf.ini", hostname);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|LOG_LOCAL4|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

int main(int argc, char *argv[])
{
	int rc, selectRetVal, n;
    size_t found, start;
    socklen_t dlen;
    char recv_message[1024];
    sockaddr_x theirDAG;
    fd_set socks;
    struct timeval timeoutval;
	vector<string> routers;

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

    // connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		return -1;
	}

	xr.setRouter(hostname);
	listRoutes("AD");

   	// open socket for route process
   	route_state.sock=Xsocket(AF_XIA, SOCK_DGRAM, 0);
   	if (route_state.sock < 0) {
   		syslog(LOG_ALERT, "Unable to create a socket");
   		exit(-1);
   	}

   	// initialize the route states (e.g., set HELLO/LSA timer, etc)
   	initRouteState();

   	// bind to the src DAG
   	if (Xbind(route_state.sock, (struct sockaddr*)&route_state.sdag, sizeof(sockaddr_x)) < 0) {
   		Graph g(&route_state.sdag);
   		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(route_state.sock);
   		exit(-1);
   	}


	time_t last_purge = time(NULL);
	while (1) {
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000; // every 0.002 sec, check if any received packets

		selectRetVal = select(route_state.sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));
			dlen = sizeof(sockaddr_x);
			n = Xrecvfrom(route_state.sock, recv_message, 1024, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
	    			perror("recvfrom");
			}

			string msg = recv_message;
			start = 0;
			found=msg.find("^");
  			if (found!=string::npos) {
  				string msg_type = msg.substr(start, found-start);
  				int type = atoi(msg_type.c_str());
				switch (type) {
					case HELLO:
						// process the incoming Hello message
    						processHello(msg.c_str());
						break;
					case LSA:
						// process the incoming LSA message
  						processLSA(msg.c_str());
						break;
					case HOST_REGISTER:
						// process the incoming host-register message
  						processHostRegister(msg.c_str());
						break;
					default:
						perror("unknown routing message");
						break;
				}
  			}

		}

		time_t now = time(NULL);
		if (now - last_purge >= 10)
		{
			last_purge = now;
			map<string, time_t>::iterator iter;

			for (iter = timeStamp.begin(); iter != timeStamp.end(); iter++)	
			{
				if (now - iter->second >= EXPIRE_TIME){
					xr.delRoute(iter->first);
					timeStamp.erase(iter);
					syslog(LOG_INFO, "purging host route for : %s", iter->first.c_str());
				}
			}
		}
    }


	return 0;
}
