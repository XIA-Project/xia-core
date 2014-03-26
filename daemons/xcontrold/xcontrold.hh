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

#define HELLO_INTERVAL 0.1
#define LSA_INTERVAL 0.3
#define SID_DISCOVERY_INTERVAL 3.0
#define SID_DECISION_INTERVAL 5.0
#define AD_LSA_INTERVAL 1
#define CALC_DIJKSTRA_INTERVAL 4
#define MAX_HOP_COUNT 50
#define MAX_SEQNUM 100000
#define MAX_XID_SIZE 100
#define SEQNUM_WINDOW 10000

#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define SID_XCONTROL "SID:1110000000000000000000000000000000001114"
#define NULL_4ID "IP:4500000000010000fafa00000000000000000000"

#define SERVICE_ARCH_FLAT 0
#define SERVICE_ARCH_CENTRAL 1 // AD controller report the distance information to the service controller
#define SERVICE_ARCH_REACTIVE 2 // AD controller send reversed discovery message to each service instance
#define SERVICE_ARCH_HIERARCHY 3

typedef struct ServiceState
{
    // public information
    int version;
    int priority;
    int weight;
    int isController; // whether this instance is also a service controller, 0 for no
    std::string controllerAddr; // the address of the service controller
    int archType; // type of architecture of this service
    std::map<std::string, int> delays; // the delays from ADs to the instance {AD:delay}

    // local information used for store decision, no re-broadcast
    bool valid; // is this candidate abandoned by local decision
    int percentage; // what is the percent of
} ServiceState;

typedef struct ServiceController
{
    std::map<std::string, ServiceState> instances; // the states for each service instances, AD : state pair
} ServiceController;

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
	int32_t hello_seq;  // hello seq number of this router 
    int32_t sid_discovery_seq;    // sid discovery sequence number of this router
    int32_t sid_decision_seq;    // sid decision sequence number of this router
	int32_t hello_lsa_ratio; // frequency ratio of hello:lsa (for timer purpose)
    int32_t hello_sid_discovery_ratio; // frequency ratio of hello:sid discovery (for timer purpose)
    int32_t hello_sid_decision_ratio; // frequency ratio of hello:sid decision (for timer purpose)
	int32_t calc_dijstra_ticks;   
	bool send_hello;  // Should a hello message be sent?
	bool send_lsa;  // Should a LSA message be sent?
    bool send_sid_discovery; // Should a sid discovery message be sent?
    bool send_sid_decision; // Should a sid decision message be sent?

	int32_t ctl_seq;	// LSA sequence number of this router
	int32_t ctl_seq_recv;	// LSA sequence number of this router

    std::map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
    std::map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry
	
    std::map<std::string, NeighborEntry> neighborTable; // map neighborHID to neighbor entry
    std::map<std::string, NeighborEntry> ADNeighborTable; // map neighborHID to neighbor entry for ADs

    std::map<std::string, NodeStateEntry> networkTable; // map DestAD to NodeState entry
    std::map<std::string, NodeStateEntry> ADNetworkTable; // map DestAD to NodeState entry for ADs
	std::map<std::string, int32_t> lastSeqTable; // router-HID to their last-seq number
	std::map<std::string, int32_t> ADLastSeqTable; // router-HID to their last-seq number for ADs

    std::map<std::string, ServiceState> LocalSidList; // services provided by this sid;
    std::map<std::string, ServiceController> LocalServiceControllers; // AD controller acts as service controllers for local SIDs that need to be the master node (runs controller) for now TODO: an independent service controller daemon
    std::map<std::string, std::map<std::string, ServiceState> > SIDADsTable; //discovery plane: what the controller discovered

} RouteState;

// initialize the route state
void initRouteState();

// send Hello message (1-hop broadcast)
int sendHello();

// send control message
int sendRoutingTable(std::string destHID, std::map<std::string, RouteEntry> routingTable);

int processMsg(std::string msg);

// returns an interface number to a neighbor HID
int interfaceNumber(std::string xidType, std::string xid);

// process an incoming Hello message
int processHello(ControlMessage msg);

// extract neighboring AD info from routing table
int extractNeighborADs(map<string, RouteEntry> routingTable);

int processRoutingTable(std::map<std::string, RouteEntry> routingTable);

int sendInterdomainLSA();

int processInterdomainLSA(ControlMessage msg);

// process a LinkStateAdvertisement message 
int processLSA(ControlMessage msg);

// SID routing service management plane: keep alive with its service controller 
int sendKeepAliveToServiceController();

int processServiceKeepAlive(ControlMessage msg);

// SID routing discovery plane: send and process a sid discovery message
int sendSidDiscovery();

int processSidDiscovery(ControlMessage msg);

// Decision plane: provide choices for routing to a SID
int processSidDecision(void);

// interpret the decision for each router and send it to them
int sendSidRoutingDecision(void);

// send routing table to each router
int sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > ADSIDsTable);

// fill in SID routing for the controller itself
int processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > ADSIDsTable);

// tool function update the SIDADsTable, add or update SID:AD pair into SID:[ADs]
int updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state);

// compute the shortest path (Dijkstra)
void populateRoutingTable(std::string srcHID, std::map<std::string, NodeStateEntry> &networkTable, std::map<std::string, RouteEntry> &routingTable);

// populates routingTable with HID entries of routers in neighboring ADs
void populateNeighboringADBorderRouterEntries(string currHID, std::map<std::string, RouteEntry> &routingTable);

// populates routingTable with AD entries from routingTableAD
void populateADEntries(std::map<std::string, RouteEntry> &routingTable, std::map<std::string, RouteEntry> routingTableAD);

// print routing table
void printRoutingTable(std::string srcHID, std::map<std::string, RouteEntry> &routingTable);

void printADNetworkTable();

// timer to send Hello and LinkStateAdvertisement messages periodically
void timeout_handler(int signum);

// read config file to get local SIDs
void set_sid_conf(const char* myhostname);
