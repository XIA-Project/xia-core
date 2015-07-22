#ifndef STAGE_UTILS_FILE
#define STAGE_UTILS_FILE

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <map>
#include <vector>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>

#include <sys/time.h>

#include "Xkeys.h"

#define MAX_XID_SIZE 100
#define RECV_BUF_SIZE 1024
#define XIA_MAX_BUF 15600 // TODO: improve later

#define CHUNKSIZE 1024 

#define REREQUEST 3
#define NUM_CHUNKS 1 // 12 is the max NUM_CHUNKS to fetch at one time for 1024 K

#define FTP_NAME "www_s.ftp.aaa.xia"
#define STAGE_SERVER_NAME "www_s.prefetch_server.aaa.xia"
#define STAGE_MANAGER_NAME "www_s.prefetch_client.aaa.xia"

#define GETSSID_CMD "iwgetid -r"

#define PURGE_DELAY_SEC 10
#define MGT_DELAY_SEC 10
#define STAGE_WIN_INIT 3
#define STAGE_WIN_PRED_DELAY_SEC 3
#define STAGE_WIN_RECENT_NUM 3
#define LOOP_DELAY_MSEC 100
#define SCAN_DELAY_MSEC 10
#define CHUNK_REQUEST_DELAY_MSEC 300


#define BLANK 0	// initilized: by registration message
#define PENDING 1 // chunk is being fetched/prefetched 
#define READY 2	// chunk is available in the local/network cache 

using namespace std;

// write the message to stdout unless in quiet mode
void say(const char *fmt, ...);

// always write the message to stdout
void warn(const char *fmt, ...);

// write the message to stdout, and exit the app
void die(int ecode, const char *fmt, ...);

char *randomString(char *buf, int size);

// format: cid1 cid2, ... cidn
vector<string> strVector(char *strs);

// format: STAGE_SERVER_NAME.getAD()
char *getStageServiceName();

// format: STAGE_MANAGER_NAME.getHID()
char *getStageManagerName();

char *getXftpName();

char *string2char(string str);

long string2long(string str);

// result the string result of system command
string execSystem(string cmd);

bool file_exists(const char *filename);

// Unix epoch time in msec
long now_msec();

string getSSID();

string getAD();

string getHID();

// block and poll until new AD is returned; used when SSID changed is detected
void getNewAD(char *old_ad);

// get the SSID name from "iwgetid -r" command; app level approach
string netConnStatus(string lastSSID);

// used when client is mobile TODO: detect network
int getReply(int sock, const char *cmd, char *reply, sockaddr_x *sa, int timeout, int tries);

int sendStreamCmd(int sock, const char *cmd);

int sayHello(int sock, const char *helloMsg);

int hearHello(int sock);

// assume there is no fallback and only one SID or the first SID encounted is the final intent
int XgetRemoteAddr(int sock, char *ad, char *hid, char *sid);

int XgetServADHID(const char *name, char *ad, char *hid);

int initDatagramClient(const char *name, struct addrinfo *ai, sockaddr_x *sa);

// make connection, instantiate src_ad, src_hid, dst_ad, dst_hid 
int initStreamClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

int registerDatagramReceiver(char* name);

// register the service with the name server and open the necessary sockets, update with XListen
int registerStreamReceiver(char *name, char *myAD, char *myHID, char *my4ID);

// bind the receiving function
void *blockListener(void *listenID, void *recvFuntion (void *));

int getIndex(string target, vector<string> pool);

// update the CID list to the local staging service
int updateManifest(int sock, vector<string> CIDs);

// stage manager setup a socket connection with in-network stage service
int registerStageService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

// xftp_client setup a socket connection with stage manager
int registerStageManager(const char *name);

// construct the msg and send to stage client
int XrequestChunkStage(int sock, const ChunkStatus *cs);

char *chunkReqDag2cid(char *dag);

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