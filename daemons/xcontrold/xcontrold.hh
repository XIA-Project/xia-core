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

#define WAKEUP_INTERVAL 0.1
#define EXPIRE_TIME_D 60
#define HELLO_INTERVAL_D 0.1
#define LSA_INTERVAL_D 0.3
#define SID_DISCOVERY_INTERVAL_D 3.0
#define SID_DECISION_INTERVAL_D 5.0
#define AD_LSA_INTERVAL_D 1
#define CALC_DIJKSTRA_INTERVAL_D 4
#define MAX_HOP_COUNT_D 50
#define MAX_SEQNUM_D 1000000
#define MAX_XID_SIZE 100
#define SEQNUM_WINDOW_D 1000
#define UPDATE_LATENCY_D 60
#define UPDATE_CONFIG_D 5
#define ENABLE_SID_CTL_D 0

#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define SID_XCONTROL "SID:1110000000000000000000000000000000001114"
#define NULL_4ID "IP:4500000000010000fafa00000000000000000000"


// predefine some decision function
#define LATENCY_FIRST  0
#define PURE_LOADBALANCE 1 
#define USER_DEFINDED_FUN_1 2 
#define USER_DEFINDED_FUN_2 3

// architectures of controllers of a SID
#define ARCH_DIST 0 //purely distributed no coordination
#define ARCH_CENT 1 //all queries are forwardded to one leader controller
#define ARCH_SYNC 2 //states are synced among controllers, every controller replies to queries


typedef struct DecisionIO // the struct for decison input and output
{
    int capacity;
    int latency;
    int percentage;

} DecisionIO;

typedef struct ServiceState
{
    // public information
    int seq;
    int priority; // to be moved to controller-only
    int capacity; // should be a controller-only info, but we still announce it
    int capacity_factor; // to be moved to controller-only
    int link_factor; // to be moved to controller-only
    int isLeader; // whether this instance is also a service controller, 0 for no
    std::string leaderAddr; // the address of the service controller
    int archType; // the architecture of the controllers
    std::map<std::string, int> delays; // the delays from ADs to the instance {AD:delay}

    // SID-controller-only info
    // the decision funtion pointer: srcAD, rate, *decisionPut
    int (*decision)(std::string, int, std::map<std::string, DecisionIO>*);

    // local information used for store decision, no re-broadcast
    double weight; // calculated weight
    bool valid; // is this candidate abandoned by local decision
    int percentage; // what is the percent of
} ServiceState;

typedef struct ServiceController
{
    std::map<std::string, ServiceState> instances; // the states for each service instances, AD : state pair
} ServiceController;

typedef struct ADPathState// The path state to an AD, network 'weather' report
{
    int delay; //in milliseconds (ms)
    int hop_count;
} ADPathState;

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
	int32_t hello_timer;  // hello seq number of this router 
    int32_t lsa_timer;  // hello seq number of this router 
    int32_t sid_discovery_timer;    // sid discovery timer of this router
    int32_t sid_discovery_seq;    // sid discovery sequence number of this router
    int32_t sid_decision_seq;    // sid decision sequence number of this router
    int32_t sid_decision_timer;    // sid decision sequence number of this router
    int32_t hello_ratio; // frequency ratio of wakeup:hello (for timer purpose)
	int32_t lsa_ratio; // frequency ratio of wakeup:lsa (for timer purpose)
    int32_t sid_discovery_ratio; // frequency ratio of wakeup:sid discovery (for timer purpose)
    int32_t sid_decision_ratio; // frequency ratio of wakeup:sid decision (for timer purpose)
	int32_t calc_dijstra_ticks;   
	bool send_hello;  // Should a hello message be sent?
	bool send_lsa;  // Should a LSA message be sent?
    bool send_sid_discovery; // Should a sid discovery message be sent?
    bool send_sid_decision; // Should a sid decision message be sent?

	int32_t ctl_seq;	// LSA sequence number of this router
	int32_t ctl_seq_recv;	// LSA sequence number of this router

    int32_t sid_ctl_seq;    // LSA sequence number of this router

    std::map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
    std::map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry
	
    std::map<std::string, NeighborEntry> neighborTable; // map neighborHID to neighbor entry
    std::map<std::string, NeighborEntry> ADNeighborTable; // map neighborHID to neighbor entry for ADs

    std::map<std::string, NodeStateEntry> networkTable; // map DestAD to NodeState entry
    std::map<std::string, NodeStateEntry> ADNetworkTable; // map DestAD to NodeState entry for ADs
	std::map<std::string, int32_t> lastSeqTable; // router-HID to their last-seq number
	std::map<std::string, int32_t> ADLastSeqTable; // router-HID to their last-seq number for ADs

    std::map<std::string, ADPathState> ADPathStates; // network 'weather' report service
    std::map<std::string, ServiceState> LocalSidList; // services provided by this AD
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

// extract neighboring AD info from LSAs
int extractNeighborADs(void);

int processRoutingTable(std::map<std::string, RouteEntry> routingTable);

int sendInterdomainLSA();

int processInterdomainLSA(ControlMessage msg);

// process a LinkStateAdvertisement message 
int processLSA(ControlMessage msg);

// SID service side coordination : keep alive with the leader controller
int sendKeepAliveToServiceControllerLeader();

int processServiceKeepAlive(ControlMessage msg);

// SID routing discovery plane: send and process a sid discovery message
int sendSidDiscovery();

int processSidDiscovery(ControlMessage msg);

// Decision plane: provide choices for routing to a SID
int processSidDecisionQuery(ControlMessage msg);

int processSidDecisionAnswer(ControlMessage msg);

int processSidDecision(void); // to be removed

// interpret the decision for each router and send it to them
int sendSidRoutingDecision(void);

// send routing table to each router
int sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);

// fill in SID routing for the controller itself
int processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);

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
void set_controller_conf(const char* myhostname);

// read config file to get local SIDs
void set_sid_conf(const char* myhostname);

// several pre-defined decision functions
int Latency_first(std::string srcAD, int rate, std::map<std::string, DecisionIO>* decsion);

int Load_balance(std::string srcAD, int rate, std::map<std::string, DecisionIO>* decsion);
