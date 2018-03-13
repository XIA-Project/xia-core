/*
** Copyright 2017 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#ifndef _RouteModule_hh
#define _RouteModule_hh

#include <syslog.h>
#include <map>
#include <string>
#include <vector>

#include "Xsocket.h"
#include "xroute.pb.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "RouterConfig.hh"
#include "../common/XIARouter.hh"

// can i eliminate these?
#define MAX_DAG_SIZE 2048
#define MAX_XID_SIZE 64

// These can hopefully go away eventually
const std::string broadcast_fid   ("FID:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
const std::string intradomain_sid ("SID:1110000000000000000000000000000000001112");
const std::string global_sid      ("SID:9999999999999999999999999999999999999999");

#define LOCAL_PORT 1510     // control port for talking to xnetjd
#define BUFFER_SIZE 16384	// read buffer

// routing table flag values
#define F_HOST         0x0001 // node is a host
#define F_CORE_ROUTER  0x0002 // node is an internal router
#define F_EDGE_ROUTER  0x0004 // node is an edge router
#define F_CONTROLLER   0x0008 // node is a controller
#define F_IP_GATEWAY   0x0010 // router is a dual stack router
#define F_STATIC_ROUTE 0x0100 // route entry was added manually and should not expire

class RouteModule {
public:
	int run();				          // thread main loop - calls handler
	void stop() { _enabled = false; } // stop the thread

protected:
	bool _enabled;              // run until false
	const char *_hostname;      // our hostname
	XIARouter _xr;              // click route table interface

	int _broadcast_sock;		// temp, sock we receive hello's on
	int _local_sock;			// control interface to xnetjd
	int _source_sock;			// socket we send messages with
	int _recv_sock;				// socket we receive flooded messages on

	sockaddr_x _broadcast_dag;	// temp, dag we receive hello's on
	sockaddr_x _controller_dag;	// dag of the network controller
	sockaddr_x _source_dag;		// dag os socket we send on
	sockaddr_x _recv_dag;		// dag we receive on (same as contoller when in controller process)

	sockaddr_in _local_sa;

	RouteModule(const char *name);

	int getXIDs(std::string &ad, std::string &hid);
	int makeLocalSocket();
	int makeSocket(Graph &g, sockaddr_x *sa);

	int readMessage(char *recv_message, struct pollfd *pfd, unsigned npfds, int *iface, bool *local = NULL);
	int sendMessage(int sock, sockaddr_x *dest, const Xroute::XrouteMsg &msg);
	int sendMessage(sockaddr_x *dest, const Xroute::XrouteMsg &msg);
	int sendMessage(sockaddr *dest, const Xroute::XrouteMsg &msg);
	int sendBroadcastMessage(const Xroute::XrouteMsg &msg) { return sendMessage(&_broadcast_dag, msg); };
	int sendControllerMessage(const Xroute::XrouteMsg &msg) { return sendMessage(&_controller_dag, msg); };
	int sendLocalMessage(const Xroute::XrouteMsg &msg) { return sendMessage((sockaddr*)&_local_sa, msg); };

	// virtual functions to be defined by the subclasses
	virtual int init() = 0;      // configure the route module
	virtual int handler() = 0; // called by the main loop
};

#endif
