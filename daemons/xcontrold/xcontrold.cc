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
#include "xcontrold.hh"
#include "dagaddr.hpp"

#define DEFAULT_NAME "controller0"
#define APPNAME "xcontrold"

char *hostname = NULL;
char *ident = NULL;

RouteState route_state;
XIARouter xr;

void timeout_handler(int signum)
{
	UNUSED(signum);

	if (route_state.hello_seq < route_state.hello_lsa_ratio) {
		// send Hello
		sendHello();
		route_state.hello_seq++;
	} else if (route_state.hello_seq >= route_state.hello_lsa_ratio) {
		// it's time to send LSA
		//sendLSA();
		// reset hello req
		route_state.hello_seq = 0;
	} else {
		syslog(LOG_ERR, "hello_seq=%d hello_lsa_ratio=%d", route_state.hello_seq, route_state.hello_lsa_ratio);
	}
	// reset the timer
	signal(SIGALRM, timeout_handler);
	ualarm((int)ceil(HELLO_INTERVAL*1000000),0);
}

// send Hello message (1-hop broadcast) with my AD and my HID to the directly connected neighbors
int sendHello()
{
	ControlMessage msg1(CTL_HELLO, route_state.myAD, route_state.myHID);
	int rc1 = msg1.send(route_state.sock, &route_state.ddag);

	// Advertize controller service
	ControlMessage msg2(CTL_HELLO, route_state.myAD, route_state.myHID);
	msg2.append(SID_XCONTROL);
	int rc2 = msg2.send(route_state.sock, &route_state.ddag);

	return (rc1 < rc2)? rc1 : rc2;
}

int sendRoutingTable(std::string destHID, std::map<std::string, RouteEntry> routingTable)
{
	ControlMessage msg(CTL_ROUTING_TABLE, route_state.myAD, route_state.myHID);

	msg.append(route_state.myAD);
	msg.append(destHID);

	msg.append(route_state.ctl_seq);

	msg.append((int)routingTable.size());

	map<string, RouteEntry>::iterator it;
	for (it = routingTable.begin(); it != routingTable.end(); it++)
	{
		msg.append(it->second.dest);
		msg.append(it->second.nextHop);
		msg.append(it->second.port);
		msg.append(it->second.flags);
	}

	route_state.ctl_seq = (route_state.ctl_seq + 1) % MAX_SEQNUM;

	return msg.send(route_state.sock, &route_state.ddag);
}

int processMsg(std::string msg)
{
    int type, rc = 0;
    ControlMessage m(msg);

    m.read(type);

    switch (type)
    {
        case CTL_HOST_REGISTER:
//            rc = processHostRegister(m);
            break;
        case CTL_HELLO:
//            rc = processHello(m);
            break;
        case CTL_LSA:
            rc = processLSA(m);
            break;
        case CTL_ROUTING_TABLE:
            // rc = processRoutingTable(m);
            break;
		case CTL_XBGP:
			rc = processXBGP(m);
			break;
        default:
            perror("unknown routing message");
            break;
    }

    return rc;
}

// process a control message 
int processRoutingTable(ControlMessage msg)
{
	/* Procedure:
		0. scan this LSA (mark AD with a DualRouter if there)
		1. filter out the already seen LSA (via LSA-seq for this dest)
		2. update the network table
		3. rebroadcast this LSA
	*/

	// 0. Read this LSA
	string srcAD, srcHID, destAD, destHID, hid, nextHop;
    int ctlSeq, numEntries, port, flags, rc;

    msg.read(srcAD);
    msg.read(srcHID);

    /* Check if this came from our controller */
    if (srcAD != route_state.myAD)
        return 1;

    msg.read(destAD);
    msg.read(destHID);

    /* Check if intended for me */
    if ((destAD != route_state.myAD) || (destHID != route_state.myHID))
        return msg.send(route_state.sock, &route_state.ddag);

    msg.read(ctlSeq);

  	// 1. Filter out the already seen LSA
    // If this LSA already seen, ignore this LSA; do nothing
    if (ctlSeq <= route_state.ctl_seq_recv && route_state.ctl_seq_recv - ctlSeq < 10000)
        return 1;

    route_state.ctl_seq_recv = ctlSeq;

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

int processXBGP(ControlMessage msg)
{
	//TODO
	syslog(LOG_INFO, "controller %s received xBGP message", route_state.myHID);
	return 1;
}

/* Procedure:
   0. scan this LSA (mark AD with a DualRouter if there)
   1. filter out the already seen LSA (via LSA-seq for this dest)
   2. update the network table
   3. rebroadcast this LSA
*/
int processLSA(ControlMessage msg)
{
	// 0. Read this LSA
	int32_t isDualRouter, numNeighbors, lastSeq;
	string srcAD, srcHID;

	msg.read(srcAD);
	msg.read(srcHID);
	msg.read(isDualRouter);

	// See if this LSA comes from AD with dualRouter
	if (isDualRouter == 1)
		route_state.dual_router_AD = srcAD;

	// First, filter out the LSA originating from myself
	if (srcHID == route_state.myHID)
		return 1;

	msg.read(lastSeq);

	// 1. Filter out the already seen LSA
	if (route_state.lastSeqTable.find(srcHID) != route_state.lastSeqTable.end()) {
		int32_t old = route_state.lastSeqTable[srcHID];
		if (lastSeq <= old && (old - lastSeq) < SEQNUM_WINDOW) {
			// drop the old LSA update.
			return 1;
		}
	}

	route_state.lastSeqTable[srcHID] = lastSeq;
	
	msg.read(numNeighbors);

	// 2. Update the network table
	NodeStateEntry entry;
	entry.ad = srcAD;
	entry.hid = srcHID;
	entry.num_neighbors = numNeighbors;

	for (int i = 0; i < numNeighbors; i++)
	{
		NeighborEntry neighbor;
		msg.read(neighbor.AD);
		msg.read(neighbor.HID);
		msg.read(neighbor.port);
		msg.read(neighbor.cost);

		entry.neighbor_list.push_back(neighbor);
	}

	route_state.networkTable[srcHID] = entry;
	route_state.calc_dijstra_ticks++;

	if (route_state.calc_dijstra_ticks >= CALC_DIJKSTRA_INTERVAL)
	{
		syslog(LOG_DEBUG, "Calcuating shortest paths\n");

		// Calculate next hop for ADs
		//std::map<std::string, RouteEntry> routingTableAD;
		//populateRoutingTable(route_state.myAD, route_state.ADTable, routingTableAD);

		// Calculate next hop for routers
		std::map<std::string, NodeStateEntry>::iterator it1;
		for (it1 = route_state.networkTable.begin(); it1 != route_state.networkTable.end(); it1++)
		{
			if ((it1->second.ad != route_state.myAD) || (it1->second.hid == ""))
				continue;

			std::map<std::string, RouteEntry> routingTable;
			populateRoutingTable(it1->second.hid, route_state.networkTable, routingTable);
			//populateADEntries(routingTable, routingTableAD);
			sendRoutingTable(it1->second.hid, routingTable);
		}

		route_state.calc_dijstra_ticks = 0;
	}

	return 1;
}

void populateADEntries(std::map<std::string, RouteEntry> &routingTable, std::map<std::string, RouteEntry> routingTableAD)
{
	std::map<std::string, RouteEntry>::iterator it1;  // Iter for route table
	
	for (it1 = routingTableAD.begin(); it1 != routingTableAD.end(); it1++) {
		string dest = it1->second.dest;
		string nextHop = it1->second.nextHop;
	}
}

void populateRoutingTable(std::string srcHID, std::map<std::string, NodeStateEntry> &networkTable, std::map<std::string, RouteEntry> &routingTable)
{
	std::map<std::string, NodeStateEntry>::iterator it1;  // Iter for network table
	std::vector<NeighborEntry>::iterator it2;             // Iter for neighbor list

	map<std::string, NodeStateEntry> unvisited;  // Set of unvisited nodes

	routingTable.clear();

	// Filter out anomalies
	//@ (When do these appear? Should they not be introduced in the first place? How about SIDs?)	
	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		if (it1->second.num_neighbors == 0 || it1->second.ad.empty() || it1->second.hid.empty()) {
			networkTable.erase(it1);
		}
	}

	unvisited = networkTable;

	// Initialize Dijkstra variables for all nodes
	for (it1=networkTable.begin(); it1 != networkTable.end(); it1++) {
		it1->second.checked = false;
		it1->second.cost = 10000000;
	}

	// Visit root node	
	string currXID;
	unvisited.erase(srcHID);
	networkTable[srcHID].checked = true;
	networkTable[srcHID].cost = 0;

	// Process neighboring nodes of root node
	for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
		currXID = (it2->AD == route_state.myAD) ? it2->HID : it2->AD;

		if (networkTable.find(currXID) != networkTable.end()) {
			networkTable[currXID].cost = it2->cost;
			networkTable[currXID].prevNode = srcHID;
		}
		else {
			// We have an endhost
			NeighborEntry neighbor;
			neighbor.AD = route_state.myAD;
			neighbor.HID = srcHID;
			//neighbor.port = 0;
			neighbor.cost = 1;

			NodeStateEntry entry;
			entry.ad = it2->AD;
			entry.hid = it2->HID;
			entry.num_neighbors = 1;
			entry.neighbor_list.push_back(neighbor);
			entry.cost = neighbor.cost;
			entry.prevNode = neighbor.HID;

			networkTable[currXID] = entry;
		}
	}

	// Loop until all nodes have been visited
	while (!unvisited.empty()) {
		int minCost = 10000000;
		string selectedHID;
		// Select unvisited node with min cost
		for (it1=unvisited.begin(); it1 != unvisited.end(); it1++) {
			currXID = (it1->second.ad == route_state.myAD) ? it1->second.hid : it1->second.ad;
			if (networkTable[currXID].cost < minCost) {
				minCost = networkTable[currXID].cost;
				selectedHID = currXID;
			}
		}
		if(selectedHID.empty()) {
			// Rest of the nodes cannot be reached from the visited set
			return;
		}

		// Remove selected node from unvisited set
		unvisited.erase(selectedHID);
		networkTable[selectedHID].checked = true;

		// Process all unvisited neighbors of selected node
		for (it2 = networkTable[selectedHID].neighbor_list.begin(); it2 != networkTable[selectedHID].neighbor_list.end(); it2++) {
			currXID = (it2->AD == route_state.myAD) ? it2->HID : it2->AD;
			if (networkTable[currXID].checked != true) {
				if (networkTable[currXID].cost > networkTable[selectedHID].cost + 1) {
					//@ Why add 1 to cost instead of using link cost from neighbor_list?
					networkTable[currXID].cost = networkTable[selectedHID].cost + 1;
					networkTable[currXID].prevNode = selectedHID;
				}
			}
		}
	}

	// For each destination ID, find the next hop ID and port by going backwards along the Dijkstra graph
	string tempHID1;			// ID of destination in srcHID's routing table
	string tempHID2;			// ID of node currently being processed
	string tempNextHopHID2;		// HID of next hop to reach destID from srcHID
	int hop_count;

	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		tempHID1 = (it1->second.ad == route_state.myAD) ? it1->second.hid : it1->second.ad;
		if (srcHID.compare(tempHID1) != 0) {
			tempHID2 = tempHID1;
			tempNextHopHID2 = it1->second.hid;
			hop_count = 0;
			while (networkTable[tempHID2].prevNode.compare(srcHID)!=0 && hop_count < MAX_HOP_COUNT) {
				tempHID2 = networkTable[tempHID2].prevNode;
				tempNextHopHID2 = networkTable[tempHID2].hid;
				hop_count++;
			}
			if (hop_count < MAX_HOP_COUNT) {
				routingTable[tempHID1].dest = tempHID1;
				routingTable[tempHID1].nextHop = tempNextHopHID2;

				// Find port of next hop
				for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
					if (((it2->AD == route_state.myAD) ? it2->HID : it2->AD) == tempHID2)
						routingTable[tempHID1].port = it2->port;
				}
			}
		}
	}

	printRoutingTable(srcHID, routingTable);
}

void printRoutingTable(std::string srcHID, std::map<std::string, RouteEntry> &routingTable)
{
	syslog(LOG_DEBUG, "Routing table for %s", srcHID.c_str());
	map<std::string, RouteEntry>::iterator it1;
	for ( it1=routingTable.begin() ; it1 != routingTable.end(); it1++ ) {
		syslog(LOG_DEBUG, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );
	}
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
	printf(" -h hostname : click device name (default=controller0)\n");
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
	int rc;
	int selectRetVal, n;
	//size_t found, start;
	socklen_t dlen;
	char recv_message[10240];
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

	// open socket for controller service
	int32_t tempSock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (tempSock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// bind to the controller service 
	struct addrinfo *ai;
	sockaddr_x tempDAG;

	if (Xgetaddrinfo(NULL, SID_XCONTROL, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to bind controller service");
		exit(-1);
	}
	memcpy(&tempDAG, ai->ai_addr, sizeof(sockaddr_x));

	if (Xbind(tempSock, (struct sockaddr*)&tempDAG, sizeof(sockaddr_x)) < 0) {
		Graph g(&tempDAG);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
perror("bind");
		Xclose(tempSock);
		exit(-1);
	}

	while (1) {
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);
		FD_SET(tempSock, &socks);
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000; // every 0.002 sec, check if any received packets

		int32_t highSock = max(route_state.sock, tempSock);
		selectRetVal = select(highSock, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));
			dlen = sizeof(sockaddr_x);
			n = Xrecvfrom(route_state.sock, recv_message, 10240, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("recvfrom");
			}

			string msg = recv_message;
            processMsg(msg);
//
//			start = 0;
//			found=msg.find("^");
//			if (found!=string::npos) {
//				string msg_type = msg.substr(start, found-start);
//				int type = atoi(msg_type.c_str());
//				switch (type) {
//					case CTL_LSA:
//						processLSA(msg);
//						break;
//					case CTL_ROUTING_TABLE:
//						processRoutingTable(msg);
//						break;
//					default:
//						break;
//				}
//			}
		}
	}

	return 0;
}
