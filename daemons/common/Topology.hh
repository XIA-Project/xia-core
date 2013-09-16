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

typedef struct {
	std::string AD;		// neigbor AD
	std::string HID;	// neighbor HID
	int32_t port;		// interface (outgoing port)
	int32_t cost; 		// link cost
    //int32_t type; // host,router, controller
} NeighborEntry;

typedef struct {
    std::string ad;
    std::string hid;
	int32_t seq; 		// LSA seq of dest (for filtering purpose)	
	int32_t num_neighbors;	// number of neighbors of dest AD //TODO: remove
    std::vector<NeighborEntry> neighbor_list; // neighbor list
	
	bool checked;	// used for calculating the shortest path
	int32_t cost;	// cost from myAD to destAD
	std::string prevNode; // previous node along the shortest path from myAD to destAD
	
} NodeStateEntry; // extracted from incoming LSA

#endif /* TOPOLOGY_HH */
