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

class Host : public RouteModule {
public:
	Host(const char *name) : RouteModule(name) {}
	~Host() {}

protected:
	// class overrides
	int handler();
	int init();

	// message handlers
	int processMsg(std::string msg, uint32_t iface);
	int processKeepalive(const Xroute::KeepaliveMsg& msg, uint32_t iface);
	int processConfig(const Xroute::ConfigMsg &msg);
	int processSIDRequest(Xroute::XrouteMsg &msg);

	int sendKeepalive();

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
	// track when to fire off lsa and keepalive msgs
	struct timeval h_freq, h_fire;

	// track the last time we saw a node in a route update, or a keepalive
	TimestampList _neighbor_timestamp;

	// last time we looked for stale entries
	time_t _last_neighbor_purge;
};

#endif
