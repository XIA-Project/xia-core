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


typedef struct _NeighborEntry {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int32_t port;		// interface (outgoing port)
	int32_t cost; 		// link cost
	time_t timestamp;   // last time updating this entry
	uint32_t flags;		// host,router, controller
	bool operator==(const struct _NeighborEntry& ne) const {
		return AD == ne.AD && HID == ne.HID && port == ne.port && cost == ne.cost;
	}
} NeighborEntry;

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
