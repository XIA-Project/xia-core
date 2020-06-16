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
#include <errno.h>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include "xrouted.hh"
#include "dagaddr.hpp"
#include "xroute.pb.h"

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
	syslog(LOG_DEBUG, "%s: route updates", xidType.c_str());
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			syslog(LOG_DEBUG, "%s: %s | %d | %s | %ld", xidType.c_str(), r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
		}
	} else if (rc == 0) {
		syslog(LOG_DEBUG, "%s: No routes exist", xidType.c_str());
	} else {
		syslog(LOG_WARNING, "%s: Error getting route list %d", xidType.c_str(), rc);
	}
	syslog(LOG_DEBUG, "%s: done listing route updates", xidType.c_str());
}


// Send Hello message containing my AD and HID to my directly connected peers
int sendHello()
{
	int buflen, rc;
	string message;

	Node n_ad(route_state.myAD);
	Node n_hid(route_state.myHID);
	Node n_sid(SID_XROUTE);

	Xroute::XrouteMsg msg;
	Xroute::HelloMsg *hello = msg.mutable_hello();
	Xroute::Node     *node  = hello->mutable_node();
	Xroute::XID      *ad    = node->mutable_ad();
	Xroute::XID      *hid   = node->mutable_hid();
	Xroute::XID      *sid   = node->mutable_sid();

	msg.set_type(Xroute::HELLO_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);
	hello->set_flags(route_state.flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);
	sid->set_type(n_sid.type());
	sid->set_id(n_sid.id(), XID_SIZE);


	msg.SerializeToString(&message);
	buflen = message.length();

	rc = Xsendto(route_state.sock, message.c_str(), buflen, 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));
	if (rc < 0) {
		// error!
		syslog(LOG_WARNING, "unable to send hello msg: %s", strerror(errno));

	} else if (rc != (int)message.length()) {
		syslog(LOG_WARNING, "ERROR sending hello. Tried sending %d bytes but rc=%d", buflen, rc);
		rc = -1;
	}

	return rc;
}

// send LinkStateAdvertisement message (flooding)
int sendLSA()
{
	int buflen, rc;
	string message;

	Node n_ad(route_state.myAD);
	Node n_hid(route_state.myHID);

	Xroute::XrouteMsg msg;
	Xroute::LSAMsg    *lsa  = msg.mutable_lsa();
	Xroute::Node      *node = lsa->mutable_node();
	Xroute::XID       *ad   = node->mutable_ad();
	Xroute::XID       *hid  = node->mutable_hid();

	msg.set_type(Xroute::LSA_MSG);
	msg.set_version(Xroute::XROUTE_PROTO_VERSION);

	lsa->set_flags(route_state.flags);
	ad ->set_type(n_ad.type());
	ad ->set_id(n_ad.id(), XID_SIZE);
	hid->set_type(n_hid.type());
	hid->set_id(n_hid.id(), XID_SIZE);

	map<std::string, NeighborEntry>::iterator it;
	for ( it=route_state.neighborTable.begin() ; it != route_state.neighborTable.end(); it++ ) {
		Node p_ad(it->second.AD);
		Node p_hid(it->second.HID);

		node = lsa->add_peers();
		ad   = node->mutable_ad();
		hid  = node->mutable_hid();

		ad ->set_type(p_ad.type());
		ad ->set_id(p_ad.id(), XID_SIZE);
		hid->set_type(p_hid.type());
		hid->set_id(p_hid.id(), XID_SIZE);
	}

	msg.SerializeToString(&message);
	buflen = message.length();

	rc = Xsendto(route_state.sock, message.c_str(), buflen, 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));
	if (rc < 0) {
		// error!
		syslog(LOG_WARNING, "unable to send lsa msg: %s", strerror(errno));

	} else if (rc != (int)message.length()) {
		syslog(LOG_WARNING, "ERROR sending lsa. Tried sending %d bytes but rc=%d", buflen, rc);
		rc = -1;
	}

	return rc;
}

// process a Host Register message
void processHostRegister(Xroute::HostJoinMsg msg)
{
	int rc;
	uint32_t flags;
	uint8_t interface = msg.interface();
	string hid = msg.hid();

//	printf("register iface:%u hid:%s\n", interface, hid.c_str());

	if (msg.has_flags()) {
		flags = msg.flags();
	} else {
		flags = F_HOST;
	}

	// FIXME: only do this if we haven't seen the HID before
	// should HIDs go into neighbor table?

	syslog(LOG_INFO, "Routing table entry: interface=%d, host=%s\n", interface, hid.c_str());
	// update the host entry in (click-side) HID table
	if ((rc = xr.setRoute(hid, interface, hid, flags)) != 0)
		syslog(LOG_ERR, "unable to set route %d", rc);

	timeStamp[hid] = time(NULL);
}

// process an incoming Hello message
int processHello(const Xroute::HelloMsg &msg, int interface)
{
	string neighborAD, neighborHID, myAD;
	uint32_t flags = 0;

	Xroute::XID xad  = msg.node().ad();
	Xroute::XID xhid = msg.node().hid();

	Node  ad(xad.type(),  xad.id().c_str(), 0);
	Node hid(xhid.type(), xhid.id().c_str(), 0);

	neighborAD  = ad. to_string();
	neighborHID = hid.to_string();

	if (msg.has_flags()) {
		flags = msg.flags();
	}

	/* Procedure:
		1. fill in the neighbor table
		2. update my entry in the networkTable
	*/
	// 1. fill in the neighbor table

	// fill in the table
	map<std::string, NeighborEntry>::iterator it;
	it=route_state.neighborTable.find(neighborAD);
	if(it == route_state.neighborTable.end()) {
		// if no entry yet
		NeighborEntry entry;
		entry.AD = neighborAD;
		entry.HID = neighborHID;
		entry.cost = 1; // for now, same cost

		entry.port = interface;

		route_state.neighborTable[neighborAD] = entry;

		// increase the neighbor count
		route_state.num_neighbors++;

		// FIXME: HACK until we get new routing daemon implemented
		// we haven't seen this AD before, so just go ahead and add a route for it's HID
		// the AD route will be set later when tables are processed
		xr.setRoute(neighborHID, interface, neighborHID, flags);
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
int processLSA(const Xroute::XrouteMsg& msg)
{
	string neighborAD, neighborHID, myAD;
	string destAD, destHID;

	// fix me once we don't need to rebroadcast the lsa
	const Xroute::LSAMsg& lsa = msg.lsa();

	Xroute::XID a = lsa.node().ad();
	Xroute::XID h = lsa.node().hid();

	Node  ad(a.type(), a.id().c_str(), 0);
	Node hid(h.type(), h.id().c_str(), 0);

	destAD  = ad.to_string();
	destHID = hid.to_string();

	// FIXME: this only allows for a single dual stack router in the network
	if (lsa.flags() & F_IP_GATEWAY) {
		route_state.dual_router_AD = destAD;
	}

	if (destHID.compare(route_state.myHID) == 0) {
		// skip if from me
		return 1;
	}

	map<std::string, NodeStateEntry>::iterator it = route_state.networkTable.find(destAD);
	if(it != route_state.networkTable.end()) {
		// For now, delete this dest AD entry in networkTable
		// (... we will re-insert the updated entry shortly)
		route_state.networkTable.erase (it);
	}

	// don't bother if there's nothing there???
	if (lsa.peers_size() == 0) {
		return 1;
	}


	// 2. Update the network table
	NodeStateEntry entry;
	entry.dest = destAD;
	entry.num_neighbors = lsa.peers_size();

	for (int i = 0; i < lsa.peers_size(); i++) {

		Node a(lsa.peers(i).ad().type(),  lsa.peers(i).ad().id().c_str(), 0);
//		Node h(lsa.peers(i).hid().type(), lsa.peers(i).hid().id().c_str(), 0);

		neighborAD  = a.to_string();
//		neighborHID = h.to_string();

		// fill the neighbors into the corresponding networkTable entry
		entry.neighbor_list.push_back(neighborAD);
	}

	route_state.networkTable[destAD] = entry;


	// printf("LSA received src=%s, num_neighbors=%d \n",
	// 	(route_state.networkTable[destAD].dest).c_str(),
	// 	route_state.networkTable[destAD].num_neighbors );


	route_state.calc_dijstra_ticks++;

	if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
		// Calculate Shortest Path algorithm
		syslog(LOG_DEBUG, "Calcuating shortest paths\n");
		calcShortestPath();
		route_state.calc_dijstra_ticks = 0;

		// update Routing table (click routing table as well)
		updateClickRoutingTable();
	}

	// 5. rebroadcast this LSA
	string message;
	msg.SerializeToString(&message);
	Xsendto(route_state.sock, message.c_str(), message.length(), 0, (struct sockaddr*)&route_state.ddag, sizeof(sockaddr_x));

	return 1;
}


void calcShortestPath() {

	// first, clear the current routing table
	route_state.ADrouteTable.clear();

	map<std::string, NodeStateEntry>::iterator it1;
	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end();) {

		// filter out an abnormal case
		if(it1->second.num_neighbors == 0 || (it1->second.dest).empty() ) {
			route_state.networkTable.erase (it1++);
		} else {
			++it1;
		}
	}


	// work on a copy of the table
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

	syslog(LOG_DEBUG, "AD Routing table at %s", route_state.myAD);
	map<std::string, RouteEntry>::iterator it1;
	for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
		syslog(LOG_DEBUG, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );

	}
}


void updateClickRoutingTable() {

	int rc, port;
	uint32_t flags;
	string destXID, nexthopXID;
	string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-");

	map<std::string, RouteEntry>::iterator it1;
	for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
		destXID = it1->second.dest;
		nexthopXID = it1->second.nextHop;
		port =  it1->second.port;
		flags = it1->second.flags;

		if ((rc = xr.setRoute(destXID, port, nexthopXID, flags)) != 0)
			syslog(LOG_ERR, "error setting route %d", rc);

		// set default AD for 4ID traffic
		if (!(route_state.flags & F_IP_GATEWAY) && destXID.compare(route_state.dual_router_AD) == 0) {
			if ((rc = xr.setRoute(default_4ID, port, nexthopXID, flags)) != 0)
				syslog(LOG_ERR, "error setting route %d", rc);
		}
	}
	listRoutes("AD");
	listRoutes("HID");
}


void initRouteState()
{
	char dag[MAX_DAG_SIZE];
	char fid[MAX_XID_SIZE];

	XcreateFID(fid, MAX_XID_SIZE);

	// make the dest DAG (broadcast to other routers)
	Graph g = Node() * Node(BFID) * Node(SID_XROUTE);
	g.fill_sockaddr(&route_state.ddag);

	syslog(LOG_INFO, "xroute Broadcast DAG: %s", g.dag_string().c_str());

	// read the localhost DAG and 4ID
	if ( XreadLocalHostAddr(route_state.sock, dag, MAX_DAG_SIZE, route_state.my4ID, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}

	// Retrieve AD and HID from highest priority path to intent
	Graph g_localhost(dag);

	// try {
	// 	route_state._myAD  = g_localhost.intent_AD();
	// 	route_state._myHID = g_localhost.intent_HID();

	// } catch (const std::exception& e) {
	// 	syslog(LOG_ALERT, "invalid source DAG: %s", e.what());
	// 	exit(-1);
	// }

	strcpy(route_state.myAD, g_localhost.intent_AD_str().c_str());
	strcpy(route_state.myHID, g_localhost.intent_HID_str().c_str());

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.sdag, ai->ai_addr, sizeof(sockaddr_x));

	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.calc_dijstra_ticks = 0;

	route_state.flags = F_EDGE_ROUTER;

	route_state.dual_router_AD = "NULL";
	// mark if this is a dual XIA-IPv4 router
	if( XisDualStackRouter(route_state.sock) == 1 ) {
		route_state.flags |= F_IP_GATEWAY;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	}
}

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
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

	if (!hostname)
		hostname = strdup(DEFAULT_NAME);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

int main(int argc, char *argv[])
{
	int rc;
	char recv_message[2048];
	sockaddr_x theirDAG;

	GOOGLE_PROTOBUF_VERIFY_VERSION;
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

	struct pollfd pfd;

	pfd.fd = route_state.sock;
	pfd.events = POLLIN;

	time_t last_purge = time(NULL);
	int iteration = 0;
	while (1) {
		iteration++;

		pfd.revents = 0;
		rc = Xpoll(&pfd, 1, MAIN_LOOP_MSEC);
		if (rc > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));

			int iface;
			struct msghdr mh;
			struct iovec iov;
			struct in_pktinfo pi;
			struct cmsghdr *cmsg;
			struct in_pktinfo *pinfo;
			char cbuf[CMSG_SPACE(sizeof pi)];

			iov.iov_base = recv_message;
			iov.iov_len = sizeof(recv_message);

			mh.msg_name = &theirDAG;
			mh.msg_namelen = sizeof(theirDAG);
			mh.msg_iov = &iov;
			mh.msg_iovlen = 1;
			mh.msg_control = cbuf;
			mh.msg_controllen = sizeof(cbuf);

			cmsg = CMSG_FIRSTHDR(&mh);
			cmsg->cmsg_level = IPPROTO_IP;
			cmsg->cmsg_type = IP_PKTINFO;
			cmsg->cmsg_len = CMSG_LEN(sizeof(pi));

			mh.msg_controllen = cmsg->cmsg_len;

			if ((rc = Xrecvmsg(route_state.sock, &mh, 0)) < 0) {
				perror("recvfrom");
				continue;
			}

			for (cmsg = CMSG_FIRSTHDR(&mh); cmsg != NULL; cmsg = CMSG_NXTHDR(&mh, cmsg)) {
				if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
					pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
					iface = pinfo->ipi_ifindex;
				}
			}


			Xroute::XrouteMsg msg;
			string smsg(recv_message, rc);

			if (!msg.ParseFromString(string(recv_message, rc))) {
				syslog(LOG_WARNING, "illegal packet received");
				continue;
			} else if (msg.version() != Xroute::XROUTE_PROTO_VERSION) {
				syslog(LOG_WARNING, "invalid version # received");
				continue;
			}

			switch (msg.type()) {
				case Xroute::HELLO_MSG:
					// process the incoming Hello message
					processHello(msg.hello(), iface);
					break;
				case Xroute::LSA_MSG:
					// process the incoming LSA message
					//processLSA(msg.lsa());
					processLSA(msg);
					break;
				case Xroute::HOST_JOIN_MSG:
					// process the incoming host-register message
					processHostRegister(msg.host_join());
					break;
				default:
					perror("unknown routing message");
					break;
			}

		} else if (rc < 0) {
			perror("Xselect failed");
			syslog(LOG_WARNING, "ERROR: Xselect returned %d", rc);
		}
		// Send HELLO every 100 ms
		if ((iteration % HELLO_ITERS) == 0) {
			// Except when we are sending an LSA
			if ((iteration % LSA_ITERS) != 0) {
				if (sendHello() < 0) {
					syslog(LOG_WARNING, "ERROR: Failed sending hello");
				}
			}
		}
		// Send an LSA every 400 ms
		if ((iteration % LSA_ITERS) == 0) {
			if (sendLSA() < 0) {
				syslog(LOG_WARNING, "ERROR: Failed sending LSA");
			}
		}

		time_t now = time(NULL);
		if (now - last_purge >= 10)
		{
			last_purge = now;
			map<string, time_t>::iterator iter;

			iter = timeStamp.begin();
			while (iter != timeStamp.end())
			{
				if (now - iter->second >= EXPIRE_TIME){
					//TODO: Re-enable route purges after xrouted
					// starts handling host registrations with routing that
					// supports multiple routers in an AD. - Nitin
					//
					// Currently, xnetjd/netjoin_session HS3 handler
					// registers host in routing table directly. In
					// addition to sending a host register msg to xrouted.
					// Since router is also registing, that is redundant.
					//
					//xr.delRoute(iter->first);
					//syslog(LOG_DEBUG, "purging host route for : %s", iter->first.c_str());
					syslog(LOG_DEBUG, "skipped purging host route for : %s", iter->first.c_str());
					timeStamp.erase(iter++);
				} else {
					++iter;
				}
			}
		}
	}

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
