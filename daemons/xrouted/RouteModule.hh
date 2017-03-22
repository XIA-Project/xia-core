#ifndef _RouteModule_hh
#define _RouteModule_hh

#include <string>
#include <vector>
#include <pthread.h>
#include "Xsocket.h"
#include "xroute.pb.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "RouterConfig.hh"
#include "../common/XIARouter.hh"

// can i eliminate these?
#define MAX_DAG_SIZE 1024
#define MAX_XID_SIZE 64

const std::string flood_fid      ("FID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
const std::string controller_fid ("FID:CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
const std::string broadcast_fid  ("FID:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
const std::string intradomain_sid("SID:1110000000000000000000000000000000001112");
const std::string controller_sid ("SID:1110000000000000000000000000000000001114");
const std::string local_sid      ("SID:1110000000000000000000000000000000001115");
const std::string lsa_sid        ("SID:9999999999999999999999999999999999999999");

// routing table flag values
#define F_HOST         0x0001 // node is a host
#define F_CORE_ROUTER  0x0002 // node is an internal router
#define F_EDGE_ROUTER  0x0004 // node is an edge router
#define F_CONTROLLER   0x0008 // node is a controller
#define F_IP_GATEWAY   0x0010 // router is a dual stack router
#define F_STATIC_ROUTE 0x0100 // route entry was added manually and should not expire

class RouteModule {
public:
	pthread_t start();              // create a new thread and start running
	static void *run(void *);       // thread main loop - calls handler
	void stop() {_enabled = false;} // stop the thread
	int wait();

protected:
	bool _enabled;
	pthread_t _t;
	const char *_hostname;
	XIARouter _xr;

	int _broadcast_sock;
	int _local_sock;
	int _source_sock;

	sockaddr_x _broadcast_dag;
	sockaddr_x _controller_dag;
	sockaddr_x _local_dag;
	sockaddr_x _source_dag;

	RouteModule(const char *name) {_hostname = name;}

	int sendMessage(sockaddr_x *dest, const Xroute::XrouteMsg &msg);
	int sendBroadcastMessage(const Xroute::XrouteMsg &msg) { return sendMessage(&_broadcast_dag, msg); };
	int sendControllerMessage(const Xroute::XrouteMsg &msg) { return sendMessage(&_controller_dag, msg); };
	int sendLocalMessage(const Xroute::XrouteMsg &msg) { return sendMessage(&_local_dag, msg); };

	// virtual functions to be defined by the subclasses
	virtual int init() = 0;      // configure the route module
	virtual void *handler() = 0; // called by the main loop
};

typedef std::vector<RouteModule*> ModuleList;

#endif
