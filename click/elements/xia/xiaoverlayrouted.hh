#ifndef CLICK_XIAOVERLAYROUTED_HH
#define CLICK_XIAOVERLAYROUTED_HH
#include <click/element.hh>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <map>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../userlevel/xroute.pb.h"

using namespace std;

CLICK_DECLS

#define F_CORE_ROUTER  0x0002 // node is an internal router
#define F_EDGE_ROUTER  0x0004 // node is an edge router
#define F_CONTROLLER   0x0008 // node is a controller
#define F_IP_GATEWAY   0x0010 // router is a dual stack router
#define F_STATIC_ROUTE 0x0100 // route entry was added manually and should not expire

// Main loop iterates every 1000 usec = 1 ms = 0.001 sec
#define MAIN_LOOP_USEC 1000
#define MAIN_LOOP_MSEC 50 // .05 sec
#define RECV_ITERS 2
#define HELLO_ITERS 2
#define LSA_ITERS 8
#define CALC_DIJKSTRA_INTERVAL 4
#define MAX_HOP_COUNT 50
#define MAX_XID_SIZE 64
#define MAX_DAG_SIZE 512

#define XR_OK					0
#define XR_NOT_CONNECTED		-1
#define XR_ALREADY_CONNECTED	-2
#define XR_CLICK_ERROR			-3
#define XR_ROUTE_EXISTS			-4
#define XR_ROUTE_DOESNT_EXIST	-5
#define XR_NOT_XIA				-6
#define XR_ROUTER_NOT_SET		-7
#define XR_BAD_HOSTNAME			-8
#define XR_INVALID_XID			-9

#define TOTAL_SPECIAL_CASES 8
#define DESTINED_FOR_DISCARD -1
#define DESTINED_FOR_LOCALHOST -2
#define DESTINED_FOR_DHCP -3
#define DESTINED_FOR_BROADCAST -4
#define REDIRECT -5
#define UNREACHABLE -6
#define FALLBACK -7

typedef struct {
	std::string xid;
	std::string nextHop;
	unsigned short port;
	unsigned long  flags;
} XIARouteEntry;

// class XIARouter {
// public:
// 	XIARouter(const char *_rtr = "router0") { _connected = false;
// 		_cserr = ControlSocketClient::no_err; _router = _rtr; };
// 	~XIARouter() { if (connected()) close(); };

// 	// connect to click
// 	int connect(std::string clickHost = "localhost", unsigned short controlPort = 7777);
// 	int connected() { return _connected; };
// 	void close();

// 	// returns the click version in <ver>
// 	int version(std::string &ver);

// 	// return a vector of router devices click knows about (host0, router1, ....)
// 	int listRouters(std::vector<std::string> &rlist);

// 	// specify which router to operate on, must be called before adding/removing routes
// 	// defaults to router0
// 	void setRouter(std::string r) { _router = r; };
// 	std::string getRouter() { return _router; };

// 	// get the current set of route entries, return value is number of entries returned or < 0 on err
// 	int getRoutes(std::string xidtype, std::vector<XIARouteEntry> &xrt);

// 	// returns 0 success, < 0 on error
// 	int addRoute(const std::string &xid, int port, const std::string &next, unsigned long flags);
// 	int setRoute(const std::string &xid, int port, const std::string &next, unsigned long flags);
// 	int delRoute(const std::string &xid);
// 	int getNeighbors(std::string xidtype, std::vector<std::string> &neighbors);

// private:
// 	bool _connected;
// 	std::string _router;
// 	int updateRoute(std::string cmd, const std::string &xid, int port, const std::string &next, unsigned long flags);
// 	string itoa(signed);
// };

typedef struct {
	std::string dest;	// destination AD or HID
	std::string nextHop;	// nexthop HID
	int32_t port;		// interface (outgoing port)
	uint32_t flags;	// flag
} RouteEntry;

typedef struct {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
  std::string SID; // neighbor SID
  std::string addr; // overlay addr
	int32_t cost; 		// link cost
	int32_t port;		// interface (outgoing port)
} NeighborEntry;


typedef struct {
	std::string dest;	// destination AD or HID
	int32_t num_neighbors;	// number of neighbors of dest AD
	std::vector<std::string> neighbor_list; // neighbor AD list

	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myAD to destAD

} NodeStateEntry; // extracted from incoming LSA

typedef struct RouteState {
	int32_t sock; // socket for routing process

	char myAD[MAX_XID_SIZE]; // this router AD
  char myOverlaySID[MAX_XID_SIZE]; // this router overlay SID
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used

	uint32_t flags;
	std::string dual_router_AD; // AD (with dual router) -- default AD for 4ID traffic
	int32_t num_neighbors; // number of neighbor routers
	int32_t hello_lsa_ratio; // frequency ratio of hello:lsa (for timer purpose)
	int32_t calc_dijstra_ticks;

	std::map<std::string, RouteEntry> ADrouteTable; // map DestAD to route entry
	std::map<std::string, RouteEntry> HIDrouteTable; // map DestHID to route entry
	std::map<std::string, NeighborEntry*> neighborTable; // map neighborAD to neighbor entry
	std::map<std::string, NodeStateEntry> networkTable; // map DestAD to NodeState entry

} RouteState;



class XIAOverlayRouted : public Element {

void add_handlers();

protected:
  static int add_neighbor(const String &conf, Element *e, void *thunk, ErrorHandler *errh);

  RouteState route_state;
  // XIARouter xr;
  String _hostname;
  int c;
  
public:

	XIAOverlayRouted();
	~XIAOverlayRouted();

	const char *class_name() const { return "XIAOverlayRouted"; }
	const char *port_count() const { return "1/4"; }
	const char *processing() const { return PUSH; }

	void push(int, Packet *);
  std::string sendHello();
  std::string sendLSA();
  int processLSA(const Xroute::XrouteMsg &msg);


  // int getNeighbors(std::vector<std::string> &neighbors);
};

CLICK_ENDDECLS
#endif
