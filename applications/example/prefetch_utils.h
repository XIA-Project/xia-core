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

//#define MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600

// write the message to stdout unless in quiet mode
void say(const char *fmt, ...);

// always write the message to stdout
void warn(const char *fmt, ...);

// write the message to stdout, and exit the app
void die(int ecode, const char *fmt, ...);

char** str_split(char* a_str, const char *a_delim);

// result the string result of system command
char* execSystem(char* cmd);

bool file_exists(const char * filename);

long now_msec();

int sendCmd(int sock, const char *cmd);

int sayHello(int sock, const char *helloMsg);

int hearHello(int sock);

// assume there is only one SID or the first SID encounted is the final intent
char* XgetRemoteSID(int sock);

int XgetNetADHID(const char *name, char *ad, char *hid);

// make connection, instantiate src_ad, src_hid, dst_ad, dst_hid 
int initializeClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

int getChunkCount(int sock, char *reply, int sz);

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *dst_ad, char *dst_hid);

int getListedChunks(int csock, FILE *fd, char *chunks, char *dst_ad, char *dst_hid);

int oldRegisterStreamReceiver(char* name, char *myAD, char *myHID, char *my4ID);

int registerStreamReceiver(char* name, char *myAD, char *myHID, char *my4ID);

void *blockListener(void *listenID, void *recvFuntion (void *));

#endif