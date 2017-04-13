#ifndef _controller_hh
#define _controller_hh

#include <syslog.h>
#include <map>
#include <vector>
#include <string>

#include "RouteModule.hh"
#include "Topology.hh"

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
//#define MAX_XID_SIZE 100
#define SEQNUM_WINDOW_D 1000
#define UPDATE_LATENCY_D 60
#define UPDATE_CONFIG_D 5
#define ENABLE_SID_CTL_D 0

// predefine some decision function
#define LATENCY_FIRST  0
#define PURE_LOADBALANCE 1
#define RATE_LOADBALANCE 2
#define USER_DEFINDED_FUN_1 3
#define USER_DEFINDED_FUN_2 4

// architectures of controllers of a SID
#define ARCH_DIST 0 //purely distributed no coordination
#define ARCH_CENT 1 //all queries are forwarded to one leader controller
#define ARCH_SYNC 2 //states are synced among controllers, every controller replies to queries


typedef struct DecisionIO // the struct for decision input and output
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
	int internal_delay; // the internal delay of this service
	std::map<std::string, int> delays; // the delays from ADs to the instance {AD:delay}

	// SID-controller-only info
	// the decision function pointer: SID, srcAD, rate, *decisionPut
	int (*decision)(std::string, std::string, int, std::map<std::string, DecisionIO>*);

	// local information used for store decision, no re-broadcast
	double weight; // calculated weight
	bool valid; // is this candidate abandoned by local decision
	int percentage; // what is the percent of
} ServiceState;

typedef struct ClientLatency
{
	std::string AD;
	int latency;
} ClientLatency;


typedef struct ServiceLeader
{
	std::map<std::string, ServiceState> instances; // the states for each service instances, AD : state pair
	std::map<std::string, int> rates; // the global view of reported rates from the client domains
	std::map<std::string, std::map<std::string, int> > latencies; // the global view of reported latencies domains, <replica: <client: latency> >
} ServiceLeader;

typedef struct ADPathState// The path state to an AD, network 'weather' report
{
	int delay; //in milliseconds (ms);

	int hop_count;
} ADPathState;

class Controller : public RouteModule {
public:
	Controller(const char *name) : RouteModule(name) {}
	~Controller() {}

protected:
	uint32_t _flags;

	std::string _myAD;
	std::string _myHID;
	Node _my_fid;

	int _controller_sock;



	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used

	int32_t _dual_router;   // 0: this router is not a dual XIA-IP router, 1: this router is a dual router
	std::string _dual_router_AD; // AD (with dual router) -- default AD for 4ID traffic
	int32_t _num_neighbors; // number of neighbor routers
	int32_t _lsa_seq;	// LSA sequence number of this router
	int32_t _sid_discovery_seq;    // sid discovery sequence number of this router
	int32_t _sid_decision_seq;    // sid decision sequence number of this router
	int32_t _calc_dijstra_ticks;

	int32_t _ctl_seq;	// LSA sequence number of this router
	int32_t _ctl_seq_recv;	// LSA sequence number of this router

	int32_t _sid_ctl_seq;    // LSA sequence number of this router

	std::map<std::string, RouteEntry> _ADrouteTable; // map DestAD to route entry
	std::map<std::string, RouteEntry> _HIDrouteTable; // map DestHID to route entry

	std::map<std::string, NeighborEntry> _neighborTable; // map neighborHID to neighbor entry
	std::map<std::string, NeighborEntry> _ADNeighborTable; // map neighborHID to neighbor entry for ADs

	std::map<std::string, NodeStateEntry> _networkTable; // map DestAD to NodeState entry
	std::map<std::string, NodeStateEntry> _ADNetworkTable; // map DestAD to NodeState entry for ADs
	std::map<std::string, int32_t> _lastSeqTable; // router-HID to their last-seq number
	std::map<std::string, int32_t> _ADLastSeqTable; // router-HID to their last-seq number for ADs

	map<string,time_t> _timeStamp;
	std::map<std::string, NodeStateEntry> _ADNetworkTable_temp;

	std::map<std::string, ADPathState> _ADPathStates; // network 'weather' report service
	std::map<std::string, ServiceState> _LocalSidList; // services provided by this AD
	std::map<std::string, ServiceLeader> _LocalServiceLeaders; // AD controller acts as service controllers for local SIDs that need to be the master node (runs controller) for now TODO: an independent service controller daemon
	std::map<std::string, std::map<std::string, ServiceState> > _SIDADsTable; //discovery plane: what the controller discovered
	std::map<std::string, int> _SIDRateTable; //record the traffic rate from local domain for each SID

	// FIXME: need to replace this, it doesn't do anything right now
	bool _send_sid_decision;
	bool _send_sid_discovery;

	// FIXME: improve these guys
	struct timeval h_freq, h_fire;
	struct timeval l_freq, l_fire;
	struct timeval sd_freq, sd_fire;
	struct timeval sq_freq, sq_fire;

	void *handler();
	int init();
	int makeSockets();


	int sendHello();
	int sendInterDomainLSA();
	int sendKeepAliveToServiceControllerLeader();
	int sendSidDiscovery();
	int sendRoutingTable(NodeStateEntry *nodeState, std::map<std::string, RouteEntry> routingTable);
	int sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);

	int processMsg(std::string msg, uint32_t iface);
	int processHello(const Xroute::HelloMsg& msg, uint32_t iface);
	int processLSA(const Xroute::LSAMsg& msg);
	int processServiceKeepAlive(const Xroute::KeepAliveMsg& msg);
	int processInterdomainLSA(const Xroute::XrouteMsg& msg);
	int processSidDecisionQuery(const Xroute::XrouteMsg& msg);
	int processSidDiscovery(const Xroute::SIDDiscoveryMsg& msg);
	int processSidDecisionAnswer(const Xroute::DecisionAnswerMsg& msg);



	int querySidDecision();
	int processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);
	int processRoutingTable(std::map<std::string, RouteEntry> routingTable);

	int Latency_first(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	int Load_balance(std::string, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	static bool compareCL(const ClientLatency &a, const ClientLatency &b);

	int Rate_load_balance(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	int processSidDecision(void); // to be deleted
	int sendSidRoutingDecision(void);
	int updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state);
	int updateADPathStates(void);
	int extractNeighborADs(void);
	void populateNeighboringADBorderRouterEntries(string currHID, std::map<std::string, RouteEntry> &routingTable);
	void populateADEntries(std::map<std::string, RouteEntry> &routingTable, std::map<std::string, RouteEntry> ADRoutingTable);
	void populateRoutingTable(std::string srcHID, std::map<std::string, NodeStateEntry> &networkTable, std::map<std::string, RouteEntry> &routingTable);
	void printRoutingTable(std::string srcHID, std::map<std::string, RouteEntry> &routingTable);
	void printADNetworkTable();
	void set_controller_conf(const char* myhostname);
	void set_sid_conf(const char* myhostname);

	static void* updatePathThread(void* updating);

};





#endif
