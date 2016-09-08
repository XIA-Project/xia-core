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
#include <math.h>
#include <set>
#include <fcntl.h>

#include "Xsocket.h"
#include "../common/XIARouter.hh"

using namespace std;

#define MAX_XID_SIZE 100

#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE "SID:1110000000000000000000000000000000001119"

#define ROUTE_XID_DEFAULT "-"

#define CALC_DIJKSTRA_INTERVAL 4
#define CHECK_CID_ENTRY_ITERS 1000
#define CHECK_NEIGHBOR_ENTRY_ITERS 500
#define MAIN_LOOP_USEC 1000
#define MAX_SEQNUM 100000
#define MAX_HOP_COUNT 50
#define MAX_DAG_SIZE 512

static unsigned short DESTINE_FOR_LOCALHOST = 65534;

typedef struct {
	int32_t cost; 			// link cost
	int32_t port;			// interface (outgoing port)
	string HID;				// neighbor HID
} NeighborEntry;

typedef struct {
	std::string dest;		// destination CID
	std::string nextHop;	// nexthop HID
	int32_t port;			// interface (outgoing port)
	uint32_t flags;			// flag 
} RouteEntry;

typedef struct {
	std::string dest;	// destination HID
	int32_t seq; 		// LSA seq of dest (for filtering purpose)	
	int32_t num_neighbors;	// number of neighbors of dest HID

	std::set<int> cid_nums;

	std::vector<std::string> dest_cids;		// cids from the destination
	std::vector<std::string> neighbor_hids;	// neighbor hids
	
	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myHID to destHID
} NodeStateEntry; // extracted from incoming LSA

typedef struct RouteState {
	int32_t sock; // socket for routing process

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; 	// this router AD
	char myHID[MAX_XID_SIZE]; 	// this router HID
	char my4ID[MAX_XID_SIZE]; 	// not used

	int32_t lsa_seq;			// LSA sequence number of this router
	int32_t calc_dijstra_ticks;   

	std::vector<string> sourceCids; 		// cids from this router
	map<std::string, NeighborEntry> neighborTable; // map neighbor HID to neighbor entry
	map<std::string, NodeStateEntry> networkTable; // map DestHID to NodeState entry

	map<std::string, RouteEntry> CIDrouteTable; // map DestCID to route entry
} RouteState;

void printNetworkTable();

void printNeighborTable();

void printRoutingTable();

void calcShortestPath();

void updateClickRoutingTable();

void cleanUpNetworkTable();


// send LSA
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
string composeLSA(vector<string> & cids);

int sendLSAHelper(int & seq, int start, int end);

int sendLSA();

// process a LinkStateAdvertisement message 
int processLSA(std::string lsa_msg);

void fillMeToMyNetworkTable();

// initialize the route state
void initRouteState();

// list all routes of a given type
void listRoutes(std::string xidType);

// returns an interface number to a neighbor CID
int interfaceNumber(std::string xidType, std::string xid);

void populateRouteState(std::vector<XIARouteEntry> & currCidRouteEntries);

void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries);