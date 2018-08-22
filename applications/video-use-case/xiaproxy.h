#ifndef __PROXY_H__
#define __PROXY_H__

#include <vector>
#include <map>
#include <float.h>

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

    char remote_host[4096];	// ip for remote host
    char remote_port[4096];	// remote port
    char remote_path[4096];	// remote resource path
	char params[MAXLINE];		// everything after '?'

	// save this stuff at the start of the connection so
	// that it doesn't change from under us
	string ad;
	string hid;
	string cdn_host;
	uint32_t bandwidth;
	uint32_t id;

} ProxyRequestCtx;

typedef struct {
	std::string     hostname;
	std::string     cdn_ad;
	std::string     cdn_hid;
	std::string     cdn_host;
	CDNStatistics   cdn_stats;
	pthread_mutex_t cdn_lock;
	uint32_t        last_bandwidth;
} ClientInfo;

typedef vector<ClientInfo> ClientState;


#endif
