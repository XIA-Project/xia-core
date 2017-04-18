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
	void *handler();
	int init();

	// message handlers
	int processMsg(std::string msg, uint32_t iface);

	int sendHello();
	int sendLSA();
	int processHello(const Xroute::HelloMsg& msg, uint32_t iface);
	int processLSA(const Xroute::XrouteMsg& msg);

	int processConfig(const Xroute::ConfigMsg &msg);
	int processHostRegister(const Xroute::HostJoinMsg& msg);
	int processHostLeave(const Xroute::HostLeaveMsg& msg);

	int processRoutingTable(const Xroute::XrouteMsg& msg);
	int processSidRoutingTable(const Xroute::XrouteMsg& msg);

	int readMessage(char *recv_message, int *iface);
	void sendMessages();
	int postJoin();
	void purge();

	std::string _myAD;
	std::string _myHID;

	int _router_sock;
	sockaddr_x _router_dag;

	bool _joined;

	NeighborTable _neighborTable;	// neighbor nodes I've discovered
	NetworkTable  _networkTable;	// Cnodes given to me by the controller

	uint32_t _flags;

	// FIXME: improve these guys
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
