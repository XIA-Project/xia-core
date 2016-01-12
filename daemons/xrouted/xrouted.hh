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
#include "Xsocket.h"
#include <time.h>
#include <signal.h>
#include <map>
#include <math.h>
#include <fcntl.h>

#include "../common/ControlMessage.hh"
#include "../common/Topology.hh"
#include "../common/XIARouter.hh"

#define WAKEUP_INTERVAL 0.5
// Main loop iterates every 1000 usec = 1 ms = 0.001 sec
#define MAIN_LOOP_USEC 1000
#define RECV_ITERS 2
#define HELLO_ITERS 100
#define LSA_ITERS 400
#define HELLO_INTERVAL 2
#define LSA_INTERVAL 5.0
#define CALC_DIJKSTRA_INTERVAL 4
#define MAX_HOP_COUNT 50
#define MAX_SEQNUM 1000000
#define SEQNUM_WINDOW 10000
#define MAX_XID_SIZE 100

#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define NULL_4ID "IP:4500000000010000fafa00000000000000000000"

#define HELLO_EXPIRE_TIME 10

typedef struct RouteState {
	int32_t sock; // socket for routing process
	
	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used
	
	int32_t dual_router;   // 0: this router is not a dual XIA-IP router, 1: this router is a dual router
	std::string dual_router_AD; // AD (with dual router) -- default AD for 4ID traffic	
	int32_t num_neighbors; // number of neighbor routers
	int32_t lsa_seq;	// LSA sequence number of this router
	int32_t hello_timer;  // hello timer of this router 
	int32_t hello_ratio;  // frequency ratio of wakeup:hello (for timer purpose)  
	int32_t lsa_timer;  // hello timer of this router 
	int32_t lsa_ratio; // frequency ratio of wakeup:lsa (for timer purpose) 
	int32_t hello_seq;  // hello seq number of this router 
	int32_t hello_lsa_ratio; // frequency ratio of hello:lsa (for timer purpose) 
	int32_t calc_dijstra_ticks;   
	bool send_hello;  // Should a hello message be sent?
	bool send_lsa;  // Should a LSA message be sent?

	int32_t ctl_seq;	// LSA sequence number of this router
	std::map<std::string, int32_t> ctl_seqs; // Control message sequences for other routers

	int32_t sid_ctl_seq;	// LSA sequence number of this router
	std::map<std::string, int32_t> sid_ctl_seqs; // Control message sequences for other routers

    std::map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
    std::map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry
	
    std::map<std::string, NeighborEntry> neighborTable; // map neighborHID to neighbor entry
	
    std::map<std::string, NodeStateEntry> networkTable; // map DestHID to NodeState entry
	std::map<std::string, int32_t> lastSeqTable; // router-HID to their last-seq number

	std::map<std::string,time_t> hello_timeStamp; // timestamp of hello
	
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

int processMsg(std::string msg);

// process a Host Register message 
int processHostRegister(ControlMessage msg);

// process an incoming Hello message
int processHello(ControlMessage msg);

// process a LinkStateAdvertisement message 
int processLSA(ControlMessage msg);

// process a control message 
int processRoutingTable(ControlMessage msg);

// process sid routing table
int processSidRoutingTable(ControlMessage msg);

// timer to send Hello and LinkStateAdvertisement messages periodically
void timeout_handler(int signum);
