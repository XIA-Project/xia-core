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
#define EXPIRE_TIME 600

//#define XR_DEBUG

#ifdef XR_DEBUG
#define XR_LOG(...) syslog(LOG_INFO, __VA_ARGS__)
#else
#define XR_LOG(...) do {} while(0)
#endif

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
			syslog(LOG_INFO, "%s: %s | %hd | %s | %lx", xidType.c_str(), r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
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

	XR_LOG("timeout(%d)", route_state.hello_seq);
	if (route_state.hello_seq < route_state.hello_lsa_ratio) {
		// send Hello
		route_state.send_hello = true;
		//sendHello();
		route_state.hello_seq++;
	} else if (route_state.hello_seq >= route_state.hello_lsa_ratio) {
		// it's time to send LSA
		route_state.send_lsa = true;
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
/* Message format (delimiter=^)
    message-type{Hello=0}
    source-AD
    source-HID
*/
int sendHello()
{
    ControlMessage msg(CTL_HELLO, route_state.myAD, route_state.myHID);
	XR_LOG("Send-Hello");

    return msg.send(route_state.sock, &route_state.ddag);
}

// send LinkStateAdvertisement message (flooding)
/* Message format (delimiter=^)
    message-type{LSA=1}
    source-AD
    source-HID
    router-type{XIA=0 or XIA-IPv4-Dual=1}
    LSA-seq-num
    num_neighbors
    neighbor1-AD
    neighbor1-HID
    neighbor2-AD
    neighbor2-HID
    ...
*/
int sendLSA()
{
    ControlMessage msg(CTL_LSA, route_state.myAD, route_state.myHID);

    msg.append(route_state.dual_router);
    msg.append(route_state.lsa_seq);
    msg.append(route_state.num_neighbors);

    std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++)
    {
        msg.append(it->second.AD);
        msg.append(it->second.HID);
        msg.append(it->second.port);
        msg.append(it->second.cost);
  	}

	XR_LOG("Send-LSA[%d]", route_state.lsa_seq);

	route_state.lsa_seq = (route_state.lsa_seq + 1) % MAX_SEQNUM;

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
            rc = processHostRegister(m);
            break;
        case CTL_HELLO:
            rc = processHello(m);
            break;
        case CTL_LSA:
            rc = processLSA(m);
            break;
        case CTL_ROUTING_TABLE:
            rc = processRoutingTable(m);
            break;
        case CTL_SID_ROUTING_TABLE:
            rc = processSidRoutingTable(m);
            break;
        default:
            perror("unknown routing message");
            break;
    }

    return rc;
}

/* Procedure:
    1. update this host entry in (click-side) HID table:
        (hostHID, interface#, hostHID, -)
*/
int processHostRegister(ControlMessage msg)
{
    NeighborEntry neighbor;
    neighbor.AD = route_state.myAD;
    msg.read(neighbor.HID);
    neighbor.port = interfaceNumber("HID", neighbor.HID);
    neighbor.cost = 1; // for now, same cost

    /* Add host to neighbor table so info can be sent to controller */
    route_state.neighborTable[neighbor.HID] = neighbor;
    route_state.num_neighbors = route_state.neighborTable.size();

	// 2. update my entry in the networkTable
    std::string myHID = route_state.myHID;

	NodeStateEntry entry;
	entry.hid = myHID;
	entry.num_neighbors = route_state.num_neighbors;

    // fill my neighbors into my entry in the networkTable
    std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++)
 		entry.neighbor_list.push_back(it->second);

	route_state.networkTable[myHID] = entry;

	// update the host entry in (click-side) HID table
	int rc;
	if ((rc = xr.setRoute(neighbor.HID, neighbor.port, neighbor.HID, 0)) != 0)
		syslog(LOG_ERR, "unable to set route %d", rc);

	timeStamp[neighbor.HID] = time(NULL);

    return 1;
}

int processHello(ControlMessage msg)
{
	string HID;
	string SID;

	/* Update neighbor table */
    NeighborEntry neighbor;
    msg.read(neighbor.AD);
    msg.read(HID);
	if (msg.read(SID) < 0) {
		neighbor.HID = HID;
	} else {
		neighbor.HID = SID;
	}
    neighbor.port = interfaceNumber("HID", HID);
    neighbor.cost = 1; // for now, same cost

    /* Index by HID if neighbor in same domain or by AD otherwise */
    bool internal = (neighbor.AD == route_state.myAD);
    route_state.neighborTable[internal ? neighbor.HID : neighbor.AD] = neighbor;
    route_state.num_neighbors = route_state.neighborTable.size();

    /* Update network table */
    std::string myHID = route_state.myHID;

	NodeStateEntry entry;
	entry.hid = myHID;
	entry.num_neighbors = route_state.num_neighbors;

    /* Add neighbors to network table entry */
    std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++)
 		entry.neighbor_list.push_back(it->second);

	route_state.networkTable[myHID] = entry;

	XR_LOG("Process-Hello[%s]", neighbor.HID.c_str());

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
    std::string srcAD;
	std::string srcHID;
	int32_t	dualRouter;
	int32_t lastSeq;

    msg.read(srcAD);
	msg.read(srcHID);
	msg.read(dualRouter);
	msg.read(lastSeq);

	XR_LOG("Process-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());

    if (srcAD != route_state.myAD)
        return 1;

	if (route_state.lastSeqTable.find(srcHID) != route_state.lastSeqTable.end()) {
		int32_t old = route_state.lastSeqTable[srcHID];
		if (lastSeq <= old && (old - lastSeq) < SEQNUM_WINDOW) {
			// drop the old LSA update.
			XR_LOG("Drop-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());
			return 1;
		}
	}

	XR_LOG("Forward-LSA: [%d] %s %s", lastSeq, srcAD.c_str(), srcHID.c_str());
	route_state.lastSeqTable[srcHID] = lastSeq;

	// 5. rebroadcast this LSA
    return msg.send(route_state.sock, &route_state.ddag);
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

        timeStamp[hid] = time(NULL);
 	}

	return 1;
}

int processSidRoutingTable(ControlMessage msg)
{
    int rc = 1;

    int ad_count = 0;
    int sid_count = 0;
    int weight = 0;
    string srcAD, srcHID, destAD, destHID;

    string AD;
    string SID;

    msg.read(srcAD);
    msg.read(srcHID);

    if (srcAD != route_state.myAD)
        return 1;

    msg.read(destAD);
    msg.read(destHID);
    /* Check if intended for me */
    if ((destAD != route_state.myAD) || (destHID != route_state.myHID))
        return msg.send(route_state.sock, &route_state.ddag);

    std::vector<XIARouteEntry> xrt;
    xr.getRoutes("AD", xrt);

    // change vector to map AD:RouteEntry for faster lookup
    std::map<std::string, XIARouteEntry> ADlookup;
    vector<XIARouteEntry>::iterator ir;
    for (ir = xrt.begin(); ir < xrt.end(); ++ir) {
        ADlookup[ir->xid] = *ir;
    }

    msg.read(ad_count);
    for ( int i = 0; i < ad_count; ++i)
    {
        msg.read(sid_count);
        msg.read(AD);
        XIARouteEntry entry = ADlookup[AD];
        for ( int j = 0; j < sid_count; ++j)
        {
            msg.read(SID);
            msg.read(weight);
            //syslog(LOG_INFO, "add route %s, %d, %s, %lu to %s", SID.c_str(), entry.port, entry.nextHop.c_str(), entry.flags, AD.c_str());
            //rc = xr.delRoute(SID);
            if (entry.xid == route_state.myAD)
            {
                rc = xr.seletiveSetRoute(SID, -2,  entry.nextHop, entry.flags, weight, AD); // use AD as index
                //SID to local AD, NOTE: no actual server to handle sid here, just put -2 instead, TODO: try to point it to a server instance
            }
            else
            {
                rc = xr.seletiveSetRoute(SID, entry.port, entry.nextHop, entry.flags, weight, AD);
            }
            if (rc < 0 )
            {
                syslog(LOG_ERR, "error setting sid route %d", rc);
            }
        }
    }

    return rc;
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
    Graph gg(&route_state.sdag);
        syslog(LOG_INFO, "xroute Source DAG: %s", gg.dag_string().c_str());

	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.lsa_seq = rand()%MAX_SEQNUM;	// LSA sequence number of this router
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
		if (route_state.send_hello == true) {
			route_state.send_hello = false;
			sendHello();
		}
		if (route_state.send_lsa == true) {
			route_state.send_lsa = false;
			sendLSA();
		}

		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000; // every 0.002 sec, check if any received packets

		selectRetVal = select(route_state.sock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));
			dlen = sizeof(sockaddr_x);
			n = Xrecvfrom(route_state.sock, recv_message, 10240, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
	    			perror("recvfrom");
			}

            std::string msg = recv_message;
            processMsg(msg);
		}

		time_t now = time(NULL);
		if (now - last_purge >= EXPIRE_TIME)
		{
			last_purge = now;
			fprintf(stderr, "checking entry\n");
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
