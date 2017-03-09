#ifndef _RouteModule_hh
#define _RouteModule_hh

#include <string>
#include <vector>
#include <pthread.h>
#include "RouterConfig.hh"
#include "../common/XIARouter.hh"


#define MAX_DAG_SIZE 512
#define MAX_XID_SIZE 64

const std::string broadcast_fid  ("FID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
const std::string intradomain_sid("SID:1110000000000000000000000000000000001112");
const std::string controller_sid ("SID:1110000000000000000000000000000000001114");

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

	RouteModule(const char *name) {_hostname = name;}

	// virtual functions to be defined by the subclasses
	virtual int init() = 0;      // configure the route module
	virtual void *handler() = 0; // called by the main loop
};

typedef std::vector<RouteModule*> ModuleList;

#endif
