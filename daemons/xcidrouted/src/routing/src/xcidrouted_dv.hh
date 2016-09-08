#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <map>
#include <set>
#include <math.h>
#include <fcntl.h>

#include "Xsocket.h"
#include "../common/XIARouter.hh"

using namespace std;

// Main loop iterates every 1000 usec = 1 ms = 0.001 sec
#define MAIN_LOOP_USEC 1000
#define RIP_MAX_HOP_COUNT 16
#define MAX_XID_SIZE 100
#define CHECK_NEIGHBOR_ENTRY_ITERS 500
#define BROADCAST_RIP_ITERS 1000
#define MAX_DAG_SIZE 512

#define ROUTE_XID_DEFAULT "-"
#define SID_XROUTE "SID:1110000000000000000000000000000000001120"

static unsigned short DESTINE_FOR_LOCALHOST = 65534;

typedef struct {
	std::string dest;	// destination AD or HID
	std::string nextHop;	// nexthop HID
	int32_t port;		// interface (outgoing port)	
	int32_t cost;
} RouteEntry;

typedef struct RouteState {
	int32_t sock; // socket for routing process
	
	sockaddr_x sdag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used

	std::vector<string> sourceCids; 			// cids from this router

	set<std::string> neighbors; 				// map neighbor HID to neighbor entry
	map<std::string, RouteEntry> CIDrouteTable; // map DestCID to route entry
	
} RouteState;

void populateRouteState(std::vector<XIARouteEntry> & routeEntries);

string constructBroadcastRIP(vector<string> & cids);

int sendBroadcastRIP(string neighborHID);

int sendBroadcastRIPHelper(string neighborHID, int start, int end);

void broadcastRIP();

void getRouteEntries(std::string xidType, std::vector<XIARouteEntry> & result);

void listRoutes(std::string xidType);

// returns an interface number to a neighbor HID
int interfaceNumber(std::string xidType, std::string xid);

// process an incoming Hello message
int processRIPUpdate(string rip_msg);

// print routing table
void printRoutingTable();

// update the click routing table
void updateClickRoutingTable();

// initialize the route state
void initRouteState();

void printNeighborTable();

void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries);