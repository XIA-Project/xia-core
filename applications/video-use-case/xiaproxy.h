#ifndef __PROXY_H__
#define __PROXY_H__

#include <time.h>
#include <float.h>
#include <stdlib.h>
#include <vector>

#include "csapp.h"
#include "utils.h"
#include "Xsocket.h"
#include "xcache.h"

#include "dagaddr.hpp"

#define XIA_DAG_URL "DAG"
#define XIA_VID_SERVICE "xia"

#define MAX_CLIENTS 30

#define CONTENT_MANIFEST 0
#define CONTENT_STREAM 1

using namespace std;

typedef struct _CDNStat {
	bool     cached;
	uint32_t size;
	uint32_t bandwidth;
	float    tput;
	float    elapsed;
} CDNStat;

typedef vector<CDNStat> CDNStats;

typedef struct _CDNData {
	uint32_t total_requests;
	CDNStats stats;

} CDNData;

typedef map<string, CDNData> CDNStatistics;


typedef struct _ProxyRequestCtx{
    int xia_sock;				// xia socket to server inside XIA
    int browser_sock;			// socket back to browser

    char remote_host[MAXLINE];	// ip for remote host
    char remote_port[MAXLINE];	// remote port
    char remote_path[MAXLINE];	// remote resource path
	char params[MAXLINE];		// everything after '?'
	uint32_t bandwidth;

} ProxyRequestCtx;

#endif
