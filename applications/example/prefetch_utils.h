#ifndef PREFETCH_UTILS_FILE
#define PREFETCH_UTILS_FILE

#include <stdio.h>
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
#define CHUNKSIZE 1024
#define REREQUEST 3

#define NUM_CHUNKS	10
#define NUM_PROMPTS	2

/*
#define MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600
*/

/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...);

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...);
/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...);

char** str_split(char* a_str, const char *a_delim);

void usage();

bool file_exists(const char * filename);

int sendCmd(int sock, const char *cmd);

// make connection, instantiate src_ad, src_hid, dst_ad, dst_hid 
int initializeClient(const char *name, char *src_ad, char *src_hid, char *dst_ad, char *dst_hid);

int getChunkCount(int sock, char *reply, int sz);

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *dst_ad, char *dst_hid);

int getListedChunks(int csock, FILE *fd, char *chunks, char *dst_ad, char *dst_hid);

int registerStreamReceiver(char* name, char *myAD, char *myHID, char *my4ID);

#endif