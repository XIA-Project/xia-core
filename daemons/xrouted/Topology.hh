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
#ifndef TOPOLOGY_HH
#define TOPOLOGY_HH

#include <string>
#include <vector>
#include <map>
#include <algorithm>

typedef std::map<std::string, time_t> TimestampList;

typedef struct {
	std::string dest;       // destination AD or HID
	std::string nextHop;    // nexthop HID
	int32_t port;           // interface (outgoing port)
	uint32_t flags;         // flag
} RouteEntry;

typedef std::map<std::string, RouteEntry> RouteTable;


typedef struct _NeighborEntry {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int32_t port;		// interface (outgoing port)
	int32_t cost; 		// link cost
	time_t timestamp;   // last time updating this entry
	uint32_t flags;		// host,router, controller
    sockaddr_x dag;     // controller dag, if neighbor is an AD, not used for hosts
	bool operator==(const struct _NeighborEntry& ne) const {
		return AD == ne.AD && HID == ne.HID && port == ne.port && cost == ne.cost;
	}
} NeighborEntry;

typedef std::map<std::string, sockaddr_x> DAGMap;

typedef std::vector<NeighborEntry> NeighborList;
typedef std::map<std::string, NeighborEntry> NeighborTable;

typedef struct {
	std::string  ad;
	std::string  hid;
	sockaddr_x   dag;
	NeighborList neighbor_list; // neighbor list

	bool         checked;	    // used for calculating the shortest path
	int32_t      cost;          // cost from myAD to destAD
	std::string  prevNode;      // previous node along the shortest path from myAD to destAD
	time_t       timestamp;     // last time updating this entry

} NodeStateEntry; // extracted from incoming LSA

typedef std::map<std::string, NodeStateEntry> NetworkTable;

#endif /* TOPOLOGY_HH */
