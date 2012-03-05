#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "XIARouter.hh"

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


#define HELLO_INTERVAL 1
#define LSA_INTERVAL 2
#define CALC_DIJKSTRA_INTERVAL 3
#define MAX_SEQNUM 100000


#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define AD2   "AD:1000000000000000000000000000000000000002"
#define AD3   "AD:1000000000000000000000000000000000000003"
#define RHID0 "HID:1100000000000000000000000000000000000000"
#define RHID1 "HID:1100000000000000000000000000000000000001"
#define RHID2 "HID:1100000000000000000000000000000000000002"
#define RHID3 "HID:1100000000000000000000000000000000000003"

#define BHID "HID:1111111111111111111111111111111111111111"

#define SID_XROUTE "SID:1110000000000000000000000000000000001112"



typedef struct {
	std::string dest;	// destination AD or HID
	std::string nextHop;	// nexthop HID
	unsigned short port;	// outgoing port
	unsigned long  flags;	// flag 
} RouteEntry;

typedef struct {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int cost; 		// link cost
} NeighborEntry;


typedef struct {
	std::string dest;	// destination AD or HID
	int seq; 		// LSA seq of dest (for filtering purpose)	
	int num_neighbors;	// number of neighbors of dest AD
	vector<std::string> neighbor_list; // neighbor AD list
	
	bool checked;	// used for calculating the shortest path
	int cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myAD to destAD
	
} NodeStateEntry; // extracted from incoming LSA


typedef struct RouteState {
	int sock; // socket for routing process
	
	char * sdag; // src DAG: this router
	char * ddag; // dest DAG: broadcast HELLO/LSA to other routers
	char * myAD; // this router AD
	char * myHID; // this router HID
	
	int num_neighbors; // number of neighbor routers
	int lsa_seq;	// LSA sequence number of this router
	int hello_seq;  // hello seq number of this router 
	int hello_lsa_ratio; // frequency ratio of hello:lsa (for timer purpose) 
	int calc_dijstra_ticks;   

	map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
	map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry
	
	map<std::string, NeighborEntry> neighborTable; // map neighborAD to neighbor entry
	
	map<std::string, NodeStateEntry> networkTable; // map DestAD to NodeState entry
	
} RouteState;


void listRoutes(std::string xidType);

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

// compute the shortest path (Dijkstra)
void calcShortestPath();

// print routing table
void printRoutingTable();

// timer to send Hello and LinkStateAdvertisement messages periodically
void timeout_handler(int signum);




