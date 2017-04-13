#ifndef _Router_hh
#define _Router_hh

#include <syslog.h>
#include <map>
#include <vector>
#include <string>

#include "RouteModule.hh"
#include "Topology.hh"


#define MAX_SEQNUM        1000000
#define SEQNUM_WINDOW     10000
#define HELLO_EXPIRE_TIME 10
#define EXPIRE_TIME       60

class Router : public RouteModule {
public:
	Router(const char *name) : RouteModule(name) {}
	~Router() {}

protected:
	std::string _myAD;
	std::string _myHID;

	int _router_sock;

	bool _joined;
	sockaddr_x _router_dag;

	int32_t _num_neighbors;	// number of neighbor routers
	int32_t _lsa_seq;		// LSA sequence number of this router

	std::map<std::string,time_t> _hello_timeStamp;			// timestamp of hello
	std::map<std::string, NeighborEntry> _neighborTable;	// map neighborHID to neighbor entry
	std::map<std::string, NodeStateEntry> _networkTable;	// map DestHID to NodeState entry
	std::map<std::string, int32_t> _lastSeqTable;			// router-HID to their last-seq number
	map<string,time_t> timeStamp;

	int32_t _sid_ctl_seq;							// LSA sequence number of this router
	std::map<std::string, int32_t> _sid_ctl_seqs;	// Control message sequences for other routers

	map<string,time_t> _timeStamp;	// FIXME: which timestamp is this?

	uint32_t _flags;

	// for new style local routers
	int32_t _ctl_seq;	// LSA sequence number of this router
	std::map<std::string, int32_t> _ctl_seqs; // Control message sequences for other routers

	void *handler();
	int init();
	int postJoin();

	int processMsg(std::string msg, uint32_t iface);


	// Intradomain Message Handlers
	int sendHello();
	int sendLSA();
	int processHello(const Xroute::HelloMsg& msg, uint32_t iface);
	int processLSA(const Xroute::XrouteMsg& msg);

	int processConfig(const Xroute::ConfigMsg &msg);
	int processHostRegister(const Xroute::HostJoinMsg& msg);


	int processRoutingTable(const Xroute::XrouteMsg& msg);
	int processSidRoutingTable(const Xroute::XrouteMsg& msg);

	// FIXME: improve these guys
	struct timeval h_freq, h_fire;
	struct timeval l_freq, l_fire;
};

#endif
