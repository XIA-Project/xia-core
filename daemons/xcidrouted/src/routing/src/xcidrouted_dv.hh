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
#include <map>
#include <set>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "Xsocket.h"
#include "../common/XIARouter.hh"
#include "../../log/logger.h"

using namespace std;

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512
#define MAX_TTL 20
#define MSG_CUTOFF 1200
#define EXPIRE_TIME 10
#define CID_ADVERT_UPDATE_RATE_PER_SEC 1

#define ROUTE_XID_DEFAULT "-"
#define SID_XROUTE_SEND "SID:1110000000000000000000000000000000001119"
#define SID_XROUTE_RECV "SID:1110000000000000000000000000000000001120"

//#define STATS_LOG

static unsigned short DESTINE_FOR_LOCALHOST = 65534;

typedef struct {
	string nextHop;		// nexthop HID
	int32_t port;		// interface (outgoing port)	
	int32_t cost;
	time_t timer;
} RouteEntry;

typedef struct RouteState {
	int32_t send_sock; 
	int32_t recv_sock; 

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used

	set<string> neighbors; 				// map neighbor HID to neighbor entry

	/* key data structure for maintaining the CID routes */
	map<string, RouteEntry> CIDrouteTable; // map DestCID to route entry
	/* key data structure for maintaining the CID routes */

	mutex mtx;           // mutex for critical section
} RouteState;

void cleanup(int);
void printRoutingTable();
void printNeighborTable();

void populateNeighborState(std::vector<XIARouteEntry> & currHidRouteEntries);
void populateRouteState(std::vector<XIARouteEntry> & routeEntries);

int interfaceNumber(std::string xidType, std::string xid);
void getRouteEntries(std::string xidType, std::vector<XIARouteEntry> & result);
double nextWaitTimeInSecond(double ratePerSecond);
size_t getTotalBytesForCIDRoutes();

void initRouteState();
void periodicJobs();
void updateClickRoutingTable();
void removeOutdatedRoutes();

string constructBroadcastRIP(vector<string> & cids);
int sendBroadcastRIPHelper(const vector<string> & cids, string neighborHID, int start, int end);
int broadcastRIP();
int processRIPUpdate(string neighborHID, string rip_msg);