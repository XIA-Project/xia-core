/**
 * Construct xiaforwardingtable hashtable header
 **/
#ifndef XIAFORWARDINGTABLE_H
#define XIAFORWARDINGTABLE_H
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "../common/XIARouter.hh"
//#include "dagaddr.hpp"

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
//#include "Xsocket.h"
#include <time.h>
#include <signal.h>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <unordered_map>
#include <iomanip>
using namespace std;

// routing table flag values
#define F_HOST         0x0001 // node is a host
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
#define TABLE_SIZE 5000009

typedef struct {
        std::string type;
        std::string xid;
        std::string port;
        std::string nextHop;
        std::string  flags;
} RouterEntry;

void add_to_table(unordered_map<string, RouterEntry>& router_table, RouterEntry entry);
void delete_from_table(unordered_map<string, RouterEntry>& router_table, string xid);
void display_table(const unordered_map<string, RouterEntry> &router_table);
int lookup_Route(unordered_map<string, RouterEntry>& _rt, string xid );
void write_table_to_file(string fpath, unordered_map<string, RouterEntry>& router_table);
void read_table_from_file(string fpath, unordered_map<string, RouterEntry>& router_table);

#endif /* XIAFORWARDINGTABLE_H */
