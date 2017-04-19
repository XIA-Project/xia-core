#ifndef _Router_hh
#define _Router_hh

#include <syslog.h>
#include <map>
#include <vector>
#include <string>

#include "RouteModule.hh"
#include "Topology.hh"

#define NEIGHBOR_EXPIRE_TIME 10
#define ROUTE_EXPIRE_TIME    60

class Router : public RouteModule {
public:
	Router(const char *name) : RouteModule(name) {}
	~Router() {}

protected:
	// class overrides
	int handler();
	int init();

	// message handlers
	int processMsg(std::string msg, uint32_t iface);

	int sendHello();
	int sendLSA();
	int processHello(const Xroute::HelloMsg& msg, uint32_t iface);

	int processConfig(const Xroute::ConfigMsg &msg);
	int processHostRegister(const Xroute::HostJoinMsg& msg);
	int processHostLeave(const Xroute::HostLeaveMsg& msg);

	int processRoutingTable(const Xroute::XrouteMsg& msg);
	int processSidRoutingTable(const Xroute::XrouteMsg& msg);

	// other stuff
	int makeSockets();
	void purge();

protected:
	// local addr, these should change to nodes
	std::string _myAD;
	std::string _myHID;

	// true once we are configured to be on the network
	bool _joined;

	// neighbor nodes I've discovered
	NeighborTable _neighborTable;

	// our route table flags we send to others
	uint32_t _flags;

	// FIXME: improve these guys
	// track when to fire off lsa and hello msgs
	struct timeval h_freq, h_fire;
	struct timeval l_freq, l_fire;

	// track the last time we saw a node in a route update, or a hello
	TimestampList _neighbor_timestamp;
	TimestampList _route_timestamp;

	// last time we looked for stale entries
	time_t _last_route_purge;
	time_t _last_neighbor_purge;
};

#endif
