#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "../common/XIARouter.hh"
#include "dagaddr.hpp"

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include <time.h>
#include <signal.h>
#include <map>
#include <math.h>
#include <fcntl.h>
using namespace std;

// routing table flag values
#define F_HOST         0x0001 // node is a host
#define F_CORE_ROUTER  0x0002 // node is an internal router
#define F_EDGE_ROUTER  0x0004 // node is an edge router
#define F_CONTROLLER   0x0008 // node is a controller
#define F_IP_GATEWAY   0x0010 // router is a dual stack router
#define F_STATIC_ROUTE 0x0100 // route entry was added manually and should not expire



// Main loop iterates every 1000 usec = 1 ms = 0.001 sec
#define MAIN_LOOP_USEC 1000
#define MAIN_LOOP_MSEC 50 // .05 sec
#define RECV_ITERS 2
#define HELLO_ITERS 2
#define LSA_ITERS 8
#define CALC_DIJKSTRA_INTERVAL 4
#define MAX_HOP_COUNT 50
#define MAX_XID_SIZE 64
#define MAX_DAG_SIZE 512

#define BFID "FID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define NULL_4ID "IP:4500000000010000fafa00000000000000000000"


typedef struct {
	std::string dest;	// destination AD or HID
	std::string nextHop;	// nexthop HID
	int32_t port;		// interface (outgoing port)
	uint32_t flags;	// flag
} RouteEntry;

typedef struct {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int32_t cost; 		// link cost
	int32_t port;		// interface (outgoing port)
} NeighborEntry;


typedef struct {
	std::string dest;	// destination AD or HID
	int32_t num_neighbors;	// number of neighbors of dest AD
	vector<std::string> neighbor_list; // neighbor AD list

	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myAD to destAD

} NodeStateEntry; // extracted from incoming LSA


typedef struct RouteState {
	int32_t sock; // socket for routing process

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used

	uint32_t flags;
	std::string dual_router_AD; // AD (with dual router) -- default AD for 4ID traffic
	int32_t num_neighbors; // number of neighbor routers
	int32_t hello_lsa_ratio; // frequency ratio of hello:lsa (for timer purpose)
	int32_t calc_dijstra_ticks;

	map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
	map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry

	map<std::string, NeighborEntry> neighborTable; // map neighborAD to neighbor entry

	map<std::string, NodeStateEntry> networkTable; // map DestAD to NodeState entry

} RouteState;


void listRoutes(std::string xidType);

// returns an interface number to a neighbor HID
int interfaceNumber(std::string xidType, std::string xid);

// initialize the route state
void initRouteState();

// send Hello message (1-hop broadcast)
int sendHello();

// send LinkStateAdvertisement message (flooding)
int sendLSA();

// process an incoming Hello message
int processHello(const char* hello_msg);

// process a LinkStateAdvertisement message
int processLSA(const char* lsa_msg);

// process a Host Register message
void processHostRegister(const char* host_register_msg);

// compute the shortest path (Dijkstra)
void calcShortestPath();

// update the click routing table
void updateClickRoutingTable();

// print routing table
void printRoutingTable();

// timer to send Hello and LinkStateAdvertisement messages periodically
void timeout_handler(int signum);
