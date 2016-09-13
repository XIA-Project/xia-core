#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <mutex>

#include "Xsocket.h"
#include "../common/XIARouter.hh"

#include "../../log/logger.h"

using namespace std;

#define ROUTE_XID_DEFAULT "-"
#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE_SEND "SID:1110000000000000000000000000000000001119"
#define SID_XROUTE_RECV "SID:1110000000000000000000000000000000001120"

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512
#define MAX_SEQNUM 10000000
#define MAX_HOP_COUNT 50
#define MAX_TTL 2
#define EXPIRE_TIME 2
#define CID_ADVERT_UPDATE_RATE_PER_SEC 1

//#define STATS_LOG

static unsigned short DESTINE_FOR_LOCALHOST = 65534;

typedef struct {
	int32_t cost; 			// link cost
	int32_t port;			// interface (outgoing port)
} NeighborEntry;

typedef struct {
	string dest;			// destination CID
	string nextHop;			// nexthop HID
	int32_t port;			// interface (outgoing port)
	int32_t cost;
} RouteEntry;

typedef struct {
	uint32_t seq; 					// LSA seq of dest (for filtering purpose)	
	set<int> cid_nums;

	uint32_t num_neighbors;			// number of neighbors of dest HID
	map<string, time_t> dest_cids;		// cids from the destination
	vector<string> neighbor_hids;		// neighbor hids
	
	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myHID to destHID
} NodeStateEntry;

typedef struct RouteState {
	int32_t send_sock; // socket for routing process
	int32_t recv_sock; // socket for routing process

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; 	// this router AD
	char myHID[MAX_XID_SIZE]; 	// this router HID
	char my4ID[MAX_XID_SIZE]; 	// not used

	uint32_t lsa_seq;			// LSA sequence number of this router

	map<std::string, NeighborEntry> neighborTable; // map neighbor HID to neighbor entry

/* Key data structure for maintaining CID route  */
	vector<string> sourceCids; 				// cids from this router
	map<string, NodeStateEntry> networkTable;  // map DestHID to NodeState entry
	map<string, RouteEntry> CIDrouteTable; 	// map DestCID to route entry
/* Key data structure for maintaining CID route  */

	mutex mtx;           // mutex for critical section
} RouteState;

void getRouteEntries(std::string xidType, std::vector<XIARouteEntry> & result);
int interfaceNumber(std::string xidType, std::string xid);
double nextWaitTimeInSecond(double ratePerSecond);

void cleanup(int);
void printNetworkTable();
void printNeighborTable();
void printRoutingTable();
size_t getTotalBytesForCIDRoutes();

void removeAbnormalNetworkTable();
void periodicJobs();
void calcShortestPath();
void removeOutdatedRoutes(const map<string, RouteEntry> & prevRoutes);
void updateClickRoutingTable(const map<string, RouteEntry> & prevRoutes);

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

int sendLSAHelper(uint32_t & seq, int start, int end);
int sendLSA();
int processLSA(std::string lsa_msg);

void initRouteState();
void populateRouteState(std::vector<XIARouteEntry> & currCidRouteEntries);
void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries);