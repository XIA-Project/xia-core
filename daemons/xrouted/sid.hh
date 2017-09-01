
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



// FIXME: use this to build new class or whatever
class section
{
//	std::map<std::string, RouteEntry> _HIDrouteTable; // map DestHID to route entry
//	std::map<std::string, NodeStateEntry> _ADNetworkTable_temp;

	std::map<std::string, ADPathState> _ADPathStates; // network 'weather' report service
	std::map<std::string, ServiceState> _LocalSidList; // services provided by this AD
	std::map<std::string, ServiceLeader> _LocalServiceLeaders; // AD controller acts as service controllers for local SIDs that need to be the master node (runs controller) for now TODO: an independent service controller daemon
	std::map<std::string, std::map<std::string, ServiceState> > _SIDADsTable; //discovery plane: what the controller discovered
	std::map<std::string, int> _SIDRateTable; //record the traffic rate from local domain for each SID

	// FIXME: need to replace this, it doesn't do anything right now
	bool _send_sid_decision;
	bool _send_sid_discovery;

	struct timeval sd_freq, sd_fire;
	struct timeval sq_freq, sq_fire;

	int sendKeepAliveToServiceControllerLeader();
	int sendSidDiscovery();
	int sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);

	int querySidDecision();
	int processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable);

	int Latency_first(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	int Load_balance(std::string, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	static bool compareCL(const ClientLatency &a, const ClientLatency &b);

	int Rate_load_balance(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision);
	int processSidDecision(void); // to be deleted
	int sendSidRoutingDecision(void);
	int updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state);
	int updateADPathStates(void);
	void set_sid_conf(const char* myhostname);
	static void* updatePathThread(void* updating);
	int processServiceKeepAlive(const Xroute::SidKeepAliveMsg& msg);
	int processSidDecisionQuery(const Xroute::XrouteMsg& msg);
	int processSidDiscovery(const Xroute::SIDDiscoveryMsg& msg);
	int processSidDecisionAnswer(const Xroute::DecisionAnswerMsg& msg);
}

