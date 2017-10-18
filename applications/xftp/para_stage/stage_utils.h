#ifndef STAGE_UTILS_FILE
#define STAGE_UTILS_FILE

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <set>
#include <vector>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
//#include "dagaddr.h"
#include "dagaddr.hpp"
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <sys/time.h>
#include <algorithm>
#include <utility>

#include "Xkeys.h"
#include "xcache.h"

#define MAX_XID_SIZE 100
#define RECV_BUF_SIZE 1024
#define XIA_MAX_BUF 15600
#define MAX_CID_NUM 3

//#define CHUNKSIZE 1024 * 512

#define REREQUEST 6
#define NUM_CHUNKS 1 // 12 is the max NUM_CHUNKS to fetch at one time for 1024 K
#define CHUNK_REQUEST_DELAY_MSEC 10

#define FTP_NAME "www_s.ftp.aaa.xia"
#define STAGE_SERVER_NAME "www_s.stage_server.aaa.xia"
#define STAGE_MANAGER_NAME "www_s.stage_manager.aaa.xia"

#define PREFETCH_SERVER_NAME "www_s.prefetch_server.aaa.xia"
#define PREFETCH_MANAGER_NAME "www_s.prefetch_client.aaa.xia"
#define UNIXMANAGERSOCK "/tmp/stage_manager.sock"
//#define GETSSID_CMD "iwgetid -r"
#define GETSSID_CMD "iwconfig wlp1s0 | grep '\\\"[a-zA-Z _0-9.-]*\\\"' -o"
#define GETSSID_CMD2 "iwconfig wlx60a44ceca928 | grep '\\\"[a-zA-Z _0-9.-]*\\\"' -o"

#define INTERFACE1 "wlp1s0"
#define INTERFACE2 "wlx60a44ceca928"
//#define GETSSID_CMD "iwconfig wlp6s0 | grep '\\\"[a-zA-Z\\_0-9.]*\\\"' -o"
#define PURGE_DELAY_SEC 10
#define MGT_DELAY_SEC 10
#define STAGE_WIN_INIT 3
#define STAGE_WIN_PRED_DELAY_SEC 3
#define STAGE_WIN_RECENT_NUM 3
#define LOOP_DELAY_MSEC 100
#define SCAN_DELAY_MSEC 10

#define BLANK 0	// initilized: by registration message
#define PENDING 1 // chunk is being fetched/prefetched
#define READY 2	// chunk is available in the local/network cache
#define IGNORE 3
#define PREFETCH 4	// chunk is available in the local/network cache
#define NS_LOOKUP_RETRY_NUM 30
#define NS_LOOKUP_WAIT_MSEC 1000

using namespace std;

// write the message to stdout unless in quiet mode
void say(const char *fmt, ...);

// always write the message to stdout
void warn(const char *fmt, ...);

// write the message to stdout, and exit the app
void die(int ecode, const char *fmt, ...);

// format: cid1 cid2, ... cidn
vector<string> strVector(char *strs);

int XmyReadLocalHostAddr(int sockfd, char *localhostAD, unsigned lenAD, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID);

// format: STAGE_SERVER_NAME.getAD()
const char *getStageServiceName();
const char *getStageServiceName2(int iface);
const char *getStageServiceName3(string AD);
// format: STAGE_MANAGER_NAME.getHID()
const char *getStageManagerName();

const char *getXftpName();

char *string2char(string str);

long string2long(string str);

// result the string result of system command
string execSystem(string cmd);

int getRTT(const char * host);

bool file_exists(const char *filename);

// Unix epoch time in msec
long now_msec();

// Unix epoch time in usec
long long now_usec();

bool isConnect();
bool isConnect2();

string getSSID();
string getSSID2();

string getAD();
string getAD2(int iface);

string getHID();

// block and poll until new AD is returned; used when SSID changed is detected
void getNewAD(char *old_ad);
void getNewAD2(int iface, char *old_ad);

//connect to wireless network
int Hello();
int Connect_SSID(int interface, char * ssid, int freq);
int Disconnect_SSID(int interface);
int Connect1(int interface, int n_ssid, int freq);
int Disconnect1(int interface);


// get the SSID name from "iwgetid -r" command; app level approach
string netConnStatus(string lastSSID);

// used when client is mobile TODO: detect network
int getReply(int sock, const char *cmd, char *reply, sockaddr_x *sa, int timeout, int tries);

int sendStreamCmd(int sock, const char *cmd);

//int sayHello(int sock, const char *helloMsg);
#define sayHello    sendStreamCmd
int hearHello(int sock);

// assume there is no fallback and only one SID or the first SID encounted is the final intent
int XgetRemoteAddr(int sock, char *ad, char *hid, char *sid);

int XgetServADHID(const char *name, char *ad, char *hid);

int initDatagramClient(const char *name, struct addrinfo *ai, sockaddr_x *sa);

// make connection, instantiate src_ad, src_hid, dst_ad, dst_hid
int initStreamClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

int registerDatagramReceiver(char* name);

int reXgetDAGbyName(const char *name, sockaddr_x *addr, socklen_t *addrlen);

// register the service with the name server and open the necessary sockets, update with XListen
int registerStreamReceiver(const char *name, char *myAD, char *myHID, char *my4ID);

// bind the receiving function
void *blockListener(void *listenID, void *recvFuntion (void *));

void *twoFunctionBlockListener(void *listenID, void *OneRecvFuntion (void *), void *TwoRecvFuntion (void *));

int getIndex(string target, vector<string> pool);

// update the CID list to the local staging service
int updateManifest(int sock, vector<string> CIDs);

// stage manager setup a socket connection with in-network stage service
int registerStageService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);
int registerMulStageService(int iface, const char *name);

// xftp_client setup a socket connection with stage manager
int registerStageManager(const char *name);

// construct the msg and send to stage client
int XrequestChunkStage(int sock, const ChunkStatus *cs);

/*
char *chunkReqDag2cid(char *dag);

char *getPrefetchManagerName();

char *getPrefetchServiceName();

int registerPrefetchManager(const char *name);

int registerPrefetchService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

int updateManifestOld(int sock, vector<string> CIDs);
*/
//add Unix Socket   --Lwy   1.20
int registerUnixStreamReceiver(const char *servername);
int UnixBlockListener(void* listenId, void* recvFuntion (void*));
int registerUnixStageManager(const char* servername);
#endif

/* reference

#define MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600

#define REQUEST_FAILED	  0x00000001
#define WAITING_FOR_CHUNK 0x00000002
#define READY_TO_READ	  0x00000004
#define INVALID_HASH	  0x00000008

typedef struct {
    int sockfd;
    int contextID;
	unsigned cachePolicy;
    unsigned cacheSize;
	unsigned ttl;
} ChunkContext;

typedef struct {
	int size;
	char cid[CID_HASH_SIZE + 1];
	int32_t ttl;
	struct timeval timestamp;
} ChunkInfo;

typedef struct {
	char* cid;
	size_t cidLen;
	int status; // 1: ready to be read, 0: waiting for chunk response, -1: failed
} ChunkStatus;

*/
