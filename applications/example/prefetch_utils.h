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

#define MAX_XID_SIZE 100
#define RECV_BUF_SIZE 1024
#define XIA_MAX_BUF 15600 // TODO: double check with Dan later why and is it a real limitation?

#define CHUNKSIZE 1024
#define REREQUEST 3
#define NUM_CHUNKS 1

#define PREFETCH_NAME "www_s.prefetch.aaa.xia"

#define GETSSID_CMD "iwgetid -r"

using namespace std;

//#define MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600

// write the message to stdout unless in quiet mode
void say(const char *fmt, ...);

// always write the message to stdout
void warn(const char *fmt, ...);

// write the message to stdout, and exit the app
void die(int ecode, const char *fmt, ...);

char **str_split(char *a_str, const char *a_delim);

char *randomString(char *buf, int size);

// format: PREFETCH_NAME.AD:"AD"
char *getPrefetchServiceName();

char *string2char(string str);

// result the string result of system command
string execSystem(string cmd);

bool file_exists(const char *filename);

// Unix epoch time in msec
long now_msec();

string getSSID();

string getAD();

// poll until new AD is returned
void getNewAD(char *old_ad);

// get the SSID name from "iwgetid -r" command
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

int getChunkCount(int sock, char *reply, int sz);

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *dst_ad, char *dst_hid);

int getListedChunks(int csock, FILE *fd, char *chunks, char *dst_ad, char *dst_hid);

int registerDatagramReceiver(char* name);

int deprecatedRegisterStreamReceiver(char *name, char *myAD, char *myHID, char *my4ID);

int registerStreamReceiver(char *name, char *myAD, char *myHID, char *my4ID);

void *blockListener(void *listenID, void *recvFuntion (void *));

int getIndex(string target, vector<string> pool);

#endif
