#ifndef __XCIDROUTED_H
#define __XCIDROUTED_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>
#include <syslog.h>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>

#include "Xkeys.h"
#include "Xsocket.h"
#include "dagaddr.h"
#include "dagaddr.hpp"
#include "XIARouter.hh"

#define HELLO 0

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512
#define HELLO_MAX_BUF_SIZE 4096
#define MAX_SEQNUM 1000000
#define MAX_TTL 2

#define INIT_WAIT_TIME_SEC 15
#define CID_ADVERT_UPDATE_RATE_PER_SEC 1

#define DEFAULT_NAME "router0"
#define APPNAME "xcidrouted"

#define ROUTE_XID_DEFAULT "-"
#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XCIDROUTE "SID:1110000000000000000000000000000000001114"

using namespace std;

class Message{
public:
	virtual void deserialize(string data) = 0;
	virtual string serialize() const = 0;
	virtual void print() const = 0;
};

class HelloMessage: Message{
public:
	HelloMessage();
	~HelloMessage();

	string serialize() const override;
	void deserialize(string data) override;
	void print() const override;
	int send();
	int recv();

	string AD;
	string HID;
	string SID;
};

class AdvertisementMessage: Message{
public:
	AdvertisementMessage();
	~AdvertisementMessage();

	string serialize() const override;
	void deserialize(string data) override;
	void print() const override;
	int send(int sock);
	int recv(int sock);

	string senderHID;			// who is the original sender
	string currSenderHID;		// who is the neighbor that forward the message
	uint32_t seq; 				// LSA seq of from sender
	uint32_t ttl;				// ttl to broadcast the advertisement
	uint32_t distance; 			// # hops to the sender

	set<string> newCIDs; 	// new CIDs in the advertisement
	set<string> delCIDs; 	// CIDs that need deletion
};

class NeighborInfo{
public:
	NeighborInfo();
	~NeighborInfo();

	string AD;
	string HID;
	int port;

	int32_t sendSock = -1;
	int32_t recvSock = -1;		
};

typedef struct {
	string dest;	 	// destination HID for the CID route enry
	string nextHop; 	// nexthop HID
	int32_t port;		// interface (outgoing port)	
	uint32_t cost;
} CIDRouteEntry;

typedef struct {
	int32_t helloSock; 		// socket for routing process
	int32_t masterSock;	// socket for receiving advertisment

	int32_t lsaSeq;	// LSA sequence number of this router

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used
	char mySID[MAX_XID_SIZE];

	set<string> localCIDs;
 	map<string, NeighborInfo> neighbors;

 	map<string, uint32_t> HID2Seq;
 	map<string, CIDRouteEntry> CIDRoutes;

 	mutex mtx;           // mutex for critical section

} RouteState;

void help(const char* name);
void config(int argc, char** argv);

double nextWaitTimeInSecond(double ratePerSecond);
int interfaceNumber(string xidType, string xid);
void getRouteEntries(string xidType, vector<XIARouteEntry> & result);

void advertiseCIDs();
void CIDAdvertiseTimer();

void registerReceiver();
void initRouteState();
int connectToNeighbor(string AD, string HID, string SID);
void printNeighborInfo();

void processHelloMessage();
void processNeighborJoin();
void processNeighborMessage(const NeighborInfo &neighbor);

#endif 