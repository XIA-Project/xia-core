#ifndef __XCIDROUTED_H
#define __XCIDROUTED_H

#include <stdio.h>
#include <string.h>
#include <limits.h>
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

#include "../../log/logger.h"

#define IO_BUF_SIZE (1024 * 1024)

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512
#define HELLO_MAX_BUF_SIZE 4096
#define MAX_TTL 20
#define HELLO_EXPIRE 10
#define CID_ADVERT_UPDATE_RATE_PER_SEC 1

#define MAX_SEQ 10000000
#define MAX_SEQ_DIFF 3

#define DEFAULT_NAME "router0"
#define APPNAME "xcidrouted"

#define ROUTE_XID_DEFAULT "-"
#define BHID "HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
#define SID_XCIDROUTE "SID:1110000000000000000000000000000000001114"

#define STATS_LOG
#define FILTER

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

class CIDMessage: Message{
public:
	virtual int send(int sock) = 0;	

	enum MsgType {
        Join, Leave, Advertise
    };

    typedef struct {
		uint32_t ttl;			// ttl to broadcast the advertisement
		uint32_t distance;		// # hops to the sender
		string senderHID;			// who is the original sender
	} CIDInfo;
};

class AdvertisementMessage: CIDMessage{
public:
	AdvertisementMessage();
	~AdvertisementMessage();

	string serialize() const override;
	void deserialize(string data) override;
	void print() const override;

	int send(int sock) override;

	uint32_t seq; 				// LSA seq of from sender
	CIDInfo info;

	set<string> newCIDs; 	// new CIDs in the advertisement
	set<string> delCIDs; 	// CIDs that need deletion
};

class NodeJoinMessage: CIDMessage{
public:
	NodeJoinMessage();
	~NodeJoinMessage();

	string serialize() const override;
	void deserialize(string data) override;
	void print() const override;

	int send(int sock) override;

	map<string, CIDInfo> CID2Info;
};

class NodeLeaveMessage: CIDMessage{
public:
	NodeLeaveMessage();
	~NodeLeaveMessage();

	string serialize() const override;
	void deserialize(string data) override;
	void print() const override;

	int send(int sock) override;

	string prevHID;					// who send this message previously
	map<string, CIDInfo> CID2Info;
};

class NeighborInfo{
public:
	NeighborInfo();
	~NeighborInfo();

	string AD;
	string HID;
	int port;
	time_t timer;

	int32_t sendSock = -1;
	int32_t recvSock = -1;		
};

typedef struct {
	string dest;	 	// destination HID for the CID route entry
	string nextHop; 	// nexthop HID
	int32_t port;		// interface (outgoing port)	
	uint32_t cost;
} CIDRouteEntry;

typedef struct {
	int32_t helloSock; 		// socket for routing process
	int32_t masterSock;	// socket for receiving advertisment

	uint32_t lsaSeq;	// LSA sequence number of this router

	sockaddr_x sdag;
	sockaddr_x ddag;

	char myAD[MAX_XID_SIZE]; // this router AD
	char myHID[MAX_XID_SIZE]; // this router HID
	char my4ID[MAX_XID_SIZE]; // not used
	char mySID[MAX_XID_SIZE];

	// for normal advertisement
	map<string, uint32_t> HID2Seq;
 	map<string, map<uint32_t, uint32_t> > HID2Seq2TTL;

 	map<string, NeighborInfo> neighbors;

 	/* key data structure for maintaining CID routes */
	set<string> localCIDs;
 #ifdef FILTER
 	map<string, CIDRouteEntry> CIDRoutesWithFilter;
 #else
 	map<string, map<string, CIDRouteEntry> > CIDRoutes;	// dest CID to sender HID to CID route entry
 #endif
 	/* key data structure for maintaining CID routes */

 	mutex mtx;           // mutex for critical section
} RouteState;

void help(const char* name);
void config(int argc, char** argv);
void cleanup(int);

double nextWaitTimeInSecond(double ratePerSecond);
int interfaceNumber(string xidType, string xid);
void getRouteEntries(string xidType, vector<XIARouteEntry> & result);
size_t getTotalBytesForCIDRoutes();

#ifdef FILTER
void cleanCIDRoutes();
#else
void setMinCostCIDRoutes(string cid);
void resetNonLocalCIDRoutes(const set<string> & delLocal);
#endif

void advertiseCIDs();
void CIDAdvertiseTimer();

void registerReceiver();
void initRouteState();
void removeOldRoutes();
int connectToNeighbor(string AD, string HID, string SID);
void printNeighborInfo();

void processHelloMessage();
NeighborInfo removeExpiredNeighbor(string neighbor);
void removeExpiredNeighbors(const vector<string>& neighbors);
void checkExpiredNeighbors();
void processNeighborConnect();
void sendNeighborJoin(const NeighborInfo &neighbor);
void sendNeighborLeave(const NeighborInfo &neighbor);

void deleteCIDRoutes(const AdvertisementMessage & msg);
void setCIDRoutes(const AdvertisementMessage & msg, const NeighborInfo &neighbor);
void setCIDRoutesWithFilter(const AdvertisementMessage & msg, const NeighborInfo &neighbor);
void deleteCIDRoutesWithFilter(const AdvertisementMessage & msg);

bool checkSequenceAndTTL(uint32_t seq, uint32_t ttl, string senderHID);
int handleAdvertisementMessage(string data, const NeighborInfo &neighbor);
int handleNodeJoinMessage(string data, const NeighborInfo &neighbor);
int handleNodeLeaveMessage(string data, const NeighborInfo &neighbor);
int handleNeighborMessage(string data, const NeighborInfo &neighbor);
int recvMessageFromSock(int sock, string &data);
int sendMessageToSock(int sock, string data);
int processNeighborMessage(const NeighborInfo &neighbor);

#endif 