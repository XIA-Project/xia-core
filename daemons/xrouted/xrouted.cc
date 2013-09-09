#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string>
#include <vector>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include "xrouted.hh"
#include "dagaddr.hpp"

#define DEFAULT_NAME "router0"
#define APPNAME "xrouted"

char *hostname = NULL;
char *ident = NULL;

RouteState route_state;

XIARouter xr;

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
// Send my AD and my HID to the directly connected neighbors
int sendHello()
{
	/* Message format (delimiter=^)
		message-type{Hello=0 or LSA=1}
		source-AD
		source-HID
	*/

    ControlMessage msg(CTL_HELLO);

    msg.append(route_state.myAD);
    msg.append(route_state.myHID);

	Xsendto(route_state.sock, msg.c_str(), msg.size(), 0, (struct sockaddr *)&route_state.ddag, sizeof(sockaddr_x));

	return 1;
}

// send LinkStateAdvertisement message (flooding)
int sendLSA()
{
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

    ControlMessage msg(CTL_LSA);

    msg.append(route_state.dual_router);
    msg.append(route_state.myAD);
    msg.append(route_state.myHID);

    msg.append(route_state.lsa_seq);
    msg.append(route_state.num_neighbors);

	map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++)
    {
        msg.append(it->second.AD);
        msg.append(it->second.HID);
        msg.append(it->second.port);
  	}

	Xsendto(route_state.sock, msg.c_str(), msg.size(), 0, (struct sockaddr *)&route_state.ddag, sizeof(sockaddr_x));

	// increase the LSA seq
	route_state.lsa_seq = (route_state.lsa_seq + 1) % MAX_SEQNUM;

	return 1;
}

// process a Host Register message
void processHostRegister(const char *host_register_msg)
{
	/* Procedure:
		1. update this host entry in (click-side) HID table:
			(hostHID, interface#, hostHID, -)
	*/
	int msgType, rc, interface;
	string hostHID;
	string neighborAD, neighborHID, myHID;

    ControlMessage msg(host_register_msg);
    msg.read(msgType);

    msg.read(hostHID);

	// update the host entry in (click-side) HID table
    interface = interfaceNumber("HID", hostHID);
	if ((rc = xr.setRoute(hostHID, interface, hostHID, 0xffff)) != 0)
		syslog(LOG_ERR, "unable to set route %d", rc);

    /* Add host to neighbor table so info can be sent to controller */
	neighborAD = route_state.myAD;
	neighborHID = hostHID;

	// fill in the table
	map<std::string, NeighborEntry>::iterator it;
	it = route_state.neighborTable.find(neighborHID);
	if (it == route_state.neighborTable.end())
    {
		// if no entry yet
		NeighborEntry entry;
		entry.AD = neighborAD;
		entry.HID = neighborHID;
		entry.cost = 1; // for now, same cost

		entry.port = interface;

		route_state.neighborTable[neighborHID] = entry;

		// increase the neighbor count
		route_state.num_neighbors++;
	}

	// 2. update my entry in the networkTable
	myHID = route_state.myHID;

    // For now, delete my entry in networkTable (... we will re-insert the updated entry shortly)
	map<std::string, NodeStateEntry>::iterator it2;
	it2 = route_state.networkTable.find(myHID);
	if (it2 != route_state.networkTable.end())
  		route_state.networkTable.erase (it2);

	NodeStateEntry entry;
	entry.dest = myHID;
	entry.seq = route_state.lsa_seq;
	entry.num_neighbors = route_state.num_neighbors;

    // fill my neighbors into my entry in the networkTable
  	map<std::string, NeighborEntry>::iterator it3;
    for (it3 = route_state.neighborTable.begin(); it3 != route_state.neighborTable.end(); it3++)
 		entry.neighbor_list.push_back(it3->second.HID);

	route_state.networkTable[myHID] = entry;
}

// process an incoming Hello message
int processHello(const char *hello_msg)
{
	/* Procedure:
		1. fill in the neighbor table
		2. update my entry in the networkTable
	*/

	// 1. fill in the neighbor table
    int msgType;
	string neighborAD, neighborHID, myHID;

    ControlMessage msg(hello_msg);
    msg.read(msgType);

    msg.read(neighborAD);
    msg.read(neighborHID);

	// fill in the table
    // if no entry yet
	map<std::string, NeighborEntry>::iterator it;
	it = route_state.neighborTable.find(neighborHID);
	if (it == route_state.neighborTable.end())
    {
		NeighborEntry entry;
		entry.AD = neighborAD;
		entry.HID = neighborHID;
		entry.cost = 1; // for now, same cost

		int interface = interfaceNumber("HID", neighborHID);
		entry.port = interface;

		route_state.neighborTable[neighborHID] = entry;

		// increase the neighbor count
		route_state.num_neighbors++;
	}

	// 2. update my entry in the networkTable
	myHID = route_state.myHID;

    // For now, delete my entry in networkTable (... we will re-insert the updated entry shortly)
	map<std::string, NodeStateEntry>::iterator it2;
	it2 = route_state.networkTable.find(myHID);
	if (it2 != route_state.networkTable.end())
  		route_state.networkTable.erase (it2);

	NodeStateEntry entry;
	entry.dest = myHID;
	entry.seq = route_state.lsa_seq;
	entry.num_neighbors = route_state.num_neighbors;

 		// fill my neighbors into my entry in the networkTable
    map<std::string, NeighborEntry>::iterator it3;
    for (it3 = route_state.neighborTable.begin(); it3 != route_state.neighborTable.end(); it3++)
 		entry.neighbor_list.push_back(it3->second.HID);

	route_state.networkTable[myHID] = entry;

	return 1;
}

// process a LinkStateAdvertisement message
int processLSA(const char *lsa_msg)
{
	/* Procedure:
		0. scan this LSA (mark AD with a DualRouter if there)
		1. filter out the already seen LSA (via LSA-seq for this dest)
		2. update the network table
		3. rebroadcast this LSA
	*/

	// 0. Read this LSA
    int msgType, isDualRouter, lsaSeq, numNeighbors, neighborPort;
    string destAD, destHID, neighborAD, neighborHID;

    ControlMessage msg(lsa_msg);
    msg.read(msgType);

    msg.read(isDualRouter);
    msg.read(destAD);
    msg.read(destHID);

  	// See if this LSA comes from AD with dualRouter
  	if (isDualRouter == 1)
  		route_state.dual_router_AD = destAD;

  	// First, filter out the LSA originating from myself
  	if (destHID == route_state.myHID)
  		return 1;

    msg.read(lsaSeq);

  	// 1. Filter out the already seen LSA
	map<std::string, NodeStateEntry>::iterator it;
	it = route_state.networkTable.find(destHID);

    // If this originating HID has been known (i.e., already in the networkTable)
	if (it != route_state.networkTable.end())
    {
        // If this LSA already seen, ignore this LSA; do nothing
  	  	if ((lsaSeq <= it->second.seq) && ((it->second.seq - lsaSeq) < 10000))
  			return 1;

  		// For now, delete this dest HID entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase (it);
  	}

    msg.read(numNeighbors);

	// 2. Update the network table
	NodeStateEntry entry;
	entry.dest = destHID;
	entry.seq = lsaSeq;
	entry.num_neighbors = numNeighbors;

  	int i;
 	for (i = 0; i < numNeighbors; i++)
    {
        msg.read(neighborAD);
        msg.read(neighborHID);
        msg.read(neighborPort);

 		// fill the neighbors into the corresponding networkTable entry
 		entry.neighbor_list.push_back(neighborHID);
 	}

	route_state.networkTable[destHID] = entry;
	route_state.calc_dijstra_ticks++;

	if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL)
    {
		// Calculate Shortest Path algorithm
		syslog(LOG_INFO, "Calcuating shortest paths\n");
		calcShortestPath();
		route_state.calc_dijstra_ticks = 0;

		// update Routing table (click routing table as well)
		//updateClickRoutingTable();
	}

	// 5. rebroadcast this LSA
	Xsendto(route_state.sock, msg.c_str(), msg.size(), 0, (struct sockaddr *)&route_state.ddag, sizeof(sockaddr_x));

	return 1;
}

void calcShortestPath() {

	// first, clear the current routing table
	route_state.HIDrouteTable.clear();

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
	string myHID, tempHID;
	myHID = route_state.myHID;
	route_state.networkTable[myHID].checked = true;
	route_state.networkTable[myHID].cost = 0;
	table.erase(myHID);

	vector<std::string>::iterator it2;
	for ( it2=route_state.networkTable[myHID].neighbor_list.begin() ; it2 < route_state.networkTable[myHID].neighbor_list.end(); it2++ ) {

		tempHID = (*it2).c_str();
		route_state.networkTable[tempHID].cost = 1;
		route_state.networkTable[tempHID].prevNode = myHID;
	}

	// loop
	while (!table.empty()) {
		int minCost = 10000000;
		string selectedHID, tmpHID;
		for ( it1=table.begin() ; it1 != table.end(); it1++ ) {
			tmpHID = it1->second.dest;
			if (route_state.networkTable[tmpHID].cost < minCost) {
				minCost = route_state.networkTable[tmpHID].cost;
				selectedHID = tmpHID;
			}
  		}
		if(selectedHID.empty()) {
			return;
		}

  		table.erase(selectedHID);
  		route_state.networkTable[selectedHID].checked = true;

 		for ( it2=route_state.networkTable[selectedHID].neighbor_list.begin() ; it2 < route_state.networkTable[selectedHID].neighbor_list.end(); it2++ ) {
			tempHID = (*it2).c_str();
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
	// set up the nexthop
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {

  		tempHID1 = it1->second.dest;
  		if ( myHID.compare(tempHID1) != 0 ) {
  			tempHID2 = tempHID1;
  			hop_count = 0;
  			while (route_state.networkTable[tempHID2].prevNode.compare(myHID)!=0 && hop_count < MAX_HOP_COUNT) {
  				tempHID2 = route_state.networkTable[tempHID2].prevNode;
  				hop_count++;
  			}
  			if(hop_count < MAX_HOP_COUNT) {
  				route_state.HIDrouteTable[tempHID1].dest = tempHID1;
  				route_state.HIDrouteTable[tempHID1].nextHop = route_state.neighborTable[tempHID2].HID;
  				route_state.HIDrouteTable[tempHID1].port = route_state.neighborTable[tempHID2].port;
  			}
  		}
  	}
	printRoutingTable();
}

// process a control message 
int processRoutingTable(const char *control_msg)
{
	/* Procedure:
		0. scan this LSA (mark AD with a DualRouter if there)
		1. filter out the already seen LSA (via LSA-seq for this dest)
		2. update the network table
		3. rebroadcast this LSA
	*/

	// 0. Read this LSA
	string destAD, destHID, hid, nextHop;
    int msgType, ctlSeq, numEntries, port, flags, rc;

    ControlMessage msg(control_msg);
    msg.read(msgType);

    msg.read(destAD);
    msg.read(destHID);

    /* Check if intended for me */
    if ((destAD != route_state.myAD) || (destHID != route_state.myHID))
    {
        Xsendto(route_state.sock, msg.c_str(), msg.size(), 0, (struct sockaddr *)&route_state.ddag, sizeof(sockaddr_x));
        return 1;
    }

    msg.read(ctlSeq);

  	// 1. Filter out the already seen LSA
    // If this LSA already seen, ignore this LSA; do nothing
    if (ctlSeq <= route_state.ctl_seq && route_state.ctl_seq - ctlSeq < 10000)
        return 1;

    route_state.ctl_seq = ctlSeq;

    msg.read(numEntries);

  	int i;
 	for (i = 0; i < numEntries; i++)
    {
        msg.read(hid);
        msg.read(nextHop);
        msg.read(port);
        msg.read(flags);

		if ((rc = xr.setRoute(hid, port, nextHop, flags)) != 0)
			syslog(LOG_ERR, "error setting route %d", rc);
 	}

	return 1;
}

void printRoutingTable() {

	syslog(LOG_INFO, "HID Routing table at %s", route_state.myHID);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.HIDrouteTable.begin() ; it1 != route_state.HIDrouteTable.end(); it1++ ) {
  		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );

  	}
}

void updateClickRoutingTable() {

	int rc, port;
	string destXID, nexthopXID;
	string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-");

  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.HIDrouteTable.begin() ; it1 != route_state.HIDrouteTable.end(); it1++ ) {
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

	route_state.ctl_seq = 0;	// LSA sequence number of this router

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
					case CTL_HELLO:
                        processHello(msg.c_str());
						break;
					case CTL_LSA:
  						processLSA(msg.c_str());
						break;
					case CTL_HOST_REGISTER:
  						processHostRegister(msg.c_str());
						break;
                    case CTL_ROUTING_TABLE:
  						processRoutingTable(msg.c_str());
						break;
					default:
						perror("unknown routing message");
						break;
				}
  			}
		}
    }

	return 0;
}
