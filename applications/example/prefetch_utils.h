#ifndef PREFETCH_UTILS_FILE
#define PREFETCH_UTILS_FILE

#include <stdio.h>
#include <iostream>
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

// TODO: make the api: fallback if not prefetch client found 
#define MAX_XID_SIZE 100
#define RECV_BUF_SIZE 1024
#define XIA_MAX_BUF 15600 // TODO: improve later

#define CHUNKSIZE 1024 

#define REREQUEST 3
#define NUM_CHUNKS 1 // 12 is the max NUM_CHUNKS to fetch at one time for 1024 K

#define FTP_NAME "www_s.ftp.aaa.xia"
#define PREFETCH_SERVER_NAME "www_s.prefetch_server.aaa.xia"
#define PREFETCH_MANAGER_NAME "www_s.prefetch_client.aaa.xia"

#define GETSSID_CMD "iwgetid -r"

#define PURGE_DELAY_SEC 120
#define MGT_DELAY_SEC 10
#define LOOP_DELAY_MSEC 100
#define SCAN_DELAY_MSEC 10
#define CHUNK_REQUEST_DELAY_MSEC 100

using namespace std;

// write the message to stdout unless in quiet mode
void say(const char *fmt, ...);

// always write the message to stdout
void warn(const char *fmt, ...);

// write the message to stdout, and exit the app
void die(int ecode, const char *fmt, ...);

char *randomString(char *buf, int size);

// format: cid1 cid2, ... cidn
vector<string> cidList(char *cids_str);

// format: PREFETCH_SERVER_NAME.getAD()
char *getPrefetchServiceName();

// format: PREFETCH_MANAGER_NAME.getHID()
char *getPrefetchManagerName();

char *getXftpName();

char *string2char(string str);

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

// update the CID list to the local prefetching service
int updateManifest(int sock, vector<string> CIDs);

// prefetch client setup a socket connection with in-network prefetch service
int registerPrefetchService(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

// xftp_client setup a socket connection with prefetch client
int registerPrefetchManager(const char *name);

// construct the msg and send to prefetch client
int XrequestChunkPrefetch(int sock, const ChunkStatus *cs);

// TODO
// int XrequestChunksAdv(int csock, const ChunkStatus *chunks, int numChunks, string serviceName, vector<string> CIDs);

#endif

/* reference 

#define MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600

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