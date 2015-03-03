#ifndef TOPOLOGY_HH
#define TOPOLOGY_HH

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include <time.h>
#include <signal.h>
#include <map>
#include <math.h>
#include <fcntl.h>

#include "ControlMessage.hh"

typedef struct {
	std::string dest;	// destination AD or HID
	std::string nextHop;	// nexthop HID
	int32_t port;		// interface (outgoing port)
	uint32_t flags;	// flag 
} RouteEntry;

struct NeighborEntry{
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int32_t port;		// interface (outgoing port)
	int32_t cost; 		// link cost
    time_t timestamp;   // last time updating this entry
    //int32_t type; // host,router, controller
    bool operator==(const struct NeighborEntry& ne) const
    {
        return AD == ne.AD && HID == ne.HID && port == ne.port && cost == ne.cost;
    }
};

typedef struct {
    std::string ad;
    std::string hid;
	int32_t num_neighbors;	// number of neighbors of dest AD //TODO: remove
    std::vector<NeighborEntry> neighbor_list; // neighbor list
	
	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myAD to destAD
    time_t timestamp; // last time updating this entry
	
} NodeStateEntry; // extracted from incoming LSA

#endif /* TOPOLOGY_HH */
