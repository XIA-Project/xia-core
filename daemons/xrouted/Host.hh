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
#ifndef _Host_hh
#define _Host_hh

#include <syslog.h>
#include <map>
#include <vector>
#include <string>

#include "RouteModule.hh"
#include "Topology.hh"

#define NEIGHBOR_EXPIRE_TIME 10

typedef struct {
	std::string ad;
	sockaddr_x  router_dag;
	sockaddr_x  source_dag;
	int         fd;
} NetInfo;

typedef std::map<uint32_t, NetInfo> NetworkMap;


class Host : public RouteModule {
public:
	Host(const char *name) : RouteModule(name) {}
	~Host() {}

protected:
	// class overrides
	int handler();
	int init();

	// message handlers
	int processMsg(std::string msg);
	int processConfig(const Xroute::HostConfigMsg &msg);

	int sendKeepalive();

	// other stuff
	int makeSock(uint32_t iface);

protected:
	// local addr, these should change to nodes
	// FIXME: need one for each interface!
	std::string _myAD;
	std::string _myHID;

	NetworkMap _networks;

	// true once we are configured to be on the network
	bool _joined;

	// our route table flags we send to others
	uint32_t _flags;

	// FIXME: improve these guys
	// track when to fire off lsa and keepalive msgs
	struct timeval h_freq, h_fire;
};

#endif
