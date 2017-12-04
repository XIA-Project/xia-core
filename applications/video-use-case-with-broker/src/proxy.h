#ifndef __PROXY_H__
#define __PROXY_H__

#include <time.h>
#include <float.h>
#include <stdlib.h>
#include <vector>
#include <mutex>
#include <string>

#include "csapp.h"
#include "utils.h"
#include "Xsocket.h"
#include "xcache.h"

#include "dagaddr.hpp"
#include "Xkeys.h"

#define TITLE "XIA Video Proxy"

#define XIA_VID_SERVICE "xia"
#define XIA_CDN_SERVICE "cdn"
#define XIA_DAG_URL "dag"
#define XIA_AD "ad"
#define XIA_HID "hid"
#define XIA_CID "cid"
#define XIA_SID "sid"

#define MAX_CLIENTS 30

#define CONTENT_MANIFEST 0
#define CONTENT_STREAM 1

// every 15 request on the same CDN, try others.
#define CDN_TRY_OTHERS_NUM_REQ 15

// if the current throughput is 40% lower than the average for a CDN, then need
// to reselect a CDN
#define CDN_RESELECT_THRESH 0.6

// the average computed should only take 0.8 of the previous running average
#define AVG_THROUGHPUT_ALPHA 0.2

// bootstrapping the average throughput for each CDN
// all video requests must make this number of requests
#define BOOTSTRAP_NUM_REQS 3

using namespace std;

/* for proxy http header back to the browser */
static const char *connection_str = "Connection: close\r\n";
static const char *http_chunk_header_status_ok = "HTTP/1.0 200 OK\r\n";
static const char *http_chunk_header_date_fmt = "Date: %a, %d %b %Y %H:%M:%S %Z\r\n";
static const char *http_chunk_header_server = "Server: XIA Video Proxy\r\n";
static const char *http_chunk_header_same_origin = "Access-Control-Allow-Origin: *\r\n";
static const char *http_header_allow_headers = "Access-Control-Allow-Headers: range\r\n";
static const char *http_header_allow_methods = "Access-Control-Allow-Methods: GET, POST, PUT\r\n";
static const char *http_chunk_header_end_marker = "\r\n";

/* http DASH header for videos */
static const char *http_chunk_header_mp4_content_type = "Content-Type: application\r\n";
static const char *http_chunk_header_mpd_content_type = "Content-Type: application\r\n";

typedef struct _CDN {
    int num_reqs;       // number of request serviced

    // running average of throughput observed for this CDN
    //
    // running average is compated as:
    //      avg_throughput = (1 - AVG_THROUGHPUT_ALPHA) * avg_throughput + AVG_THROUGHPUT_ALPHA * curr_throughput
    double avg_throughput;
} CDN;

typedef struct _ProxyRequestCtx {
    int xia_sock;               // xia socket to server inside XIA
    int browser_sock;           // socket back to browser

    char remote_host[MAXLINE];  // ip for remote host
    char remote_port[MAXLINE];  // remote port
    char remote_path[MAXLINE];  // remote resource path

    int broker_sock;            // current video request's connection with broker
    std::mutex *cdn_mutex;      // mutex that is used to protect cdn field
    std::string client_name;    // the name of client that is fetching video chunks
    std::string *cdn;           // pointer to cdn choice
} ProxyRequestCtx;

// print the usage for the proxy
void usage();

// if user stop the proxy, clean up resources
void cleanup(int sig);

// close the browser socket if done with current request
void close_fd(int browser_sock);

/*
 * parse_host_port - parse host:port (:port optional)to two parts
 */
void parse_host_port(char *host_port, char *remote_host, char *remote_port);

/**
 * Given a list of DAG urls, process it to list of DAGs
 * @param dagUrls        list of dag urls
 * @param chunkAddresses DAG addresses
 */
void process_urls_to_DAG(vector<string> & dagUrls, sockaddr_x* chunkAddresses);

/**
 * Proxy job when request comes
 * @param browser_sock socket to a given request
 * @param set          for clean up when done with request
 * @param i            index of socket of the request
 */
void job(int browser_sock, fd_set *set, int i);

/**
 * send the command in cmd to xia socket
 */
int send_command(ProxyRequestCtx *ctx, const char* cmd);

/**
 * get the reply back from the server and save it in reply var
 */
int get_server_reply(ProxyRequestCtx *ctx, char* reply, int sz);

/**
 * send the cmd to xia server and receive reply in reply.
 */
int send_and_receive_reply(ProxyRequestCtx *ctx, char* cmd , char* reply);

/**
 * get the xia socket to service identified by the sname
 * @param  sname service name
 */
int get_xia_socket_for_request(const char* sname);

/**
 * hacky way to allow cross origin requests in javascript
 */
int handle_cross_origin_probe(ProxyRequestCtx *ctx);

/**
 * handle the request for video manifest files
 */
int handle_manifest_requests(ProxyRequestCtx *ctx);

/**
 * handle the request for video chunks in DAG url
 */
int handle_stream_requests(ProxyRequestCtx *ctx);

/**
 * main part of proxy jobs, include video segment request handling and
 * manifest request handling
 *
 * @param  browser_sock socket to browser so the chunk can be sent back.
 */
int xia_proxy_handle_request(int browser_sock);

/**
 * sent back the chunks in DAG identified by chunkAddresses back to the browser with
 * the socket back to the browser.
 *
 * @param  ctx            has the browser socket
 * @param  chunkAddresses DAG addresses of the chunks
 */
int forward_chunks_to_client(ProxyRequestCtx *ctx, sockaddr_x* chunkAddresses, int numChunks, bool cdn, const char *cdn_choosed = nullptr);

/**
 * sent back the HTTP header for video chunks
 */
int forward_http_header_to_client(ProxyRequestCtx *ctx, int type);

/**
 * sent back the HTTP response body back to the client
 */
int forward_http_response_body_to_client(ProxyRequestCtx *ctx, char* data, int len);

/**
 * parse the HTTP request header in buf
 * @param  buf       buf that contains HTTP request header
 * @param  method    HTTP header method section, should be GET
 * @param  protocol  HTTP protocol
 * @param  host_port HTTP host:port
 * @param  resource  HTTP resource section
 * @param  version   HTTP version number
 */
int parse_request_line(char *buf, char *method, char *protocol, char *host_port, char *resource, char *version);

/**
 * XID in dag url from browser is not capitalized. So capitalize here.
 * @param  dagUrl dag url from browser
 */
string capitalize_XID(string dagUrl);

/**
 * given CID, AD, HID, construct a DAG with CID as intent and
 *  AD:HID:CID as fallback
 */
Graph cid2addr(std::string CID, std::string AD, std::string HID);

/**
 * given CDN service name and CID string, resolve the CDN service name
 * to the location of service. Then use the CID string to construct the DAG
 * to fetch the CID.
 */
vector<string> cdn_name_to_dag_urls(char* sname, char* cidString);

/**
 * given a list of CDN options, select one from the list
 * @param  options list of CDNs
 * @param  origin  origin CDN in case all else is not satified
 * @return         selected CDN
 */
string multicdn_select_cdn_strategy(string origin, const vector<string> & options);

/**
 * given the multi-cdn name, resolve it to a cdn name for multi-cdn
 * use case.
 * @param  sname multi-cdn service name
 * @param  pname path for the request
 */
string multicdn_name_to_CDN_name(char* origin, char* options);

/**
 * split a string based on delimiter and put them all in a list.
 */
vector<string> split_string_on_delimiter(char* str, char* delimiter);

/**
 * parse cdn list from the manifest file
 * @param  data manifest file content that contains cdn options
 * @param cdn_list list that is used to store cdn options
 */
void get_cdn_list(const char *data, vector<string> &cdn_list);

/**
 * get a request context structure that contains all the information about
 * current video request
 * @param  remote_host string that contains request id
 * @return             a new request context structure if this is a new video
 *                     request, previous request context structure that is mapped
 *                     to current request
 */
ProxyRequestCtx *get_ctx(char *remote_host);

/**
 * helper function that is used to replace request ID field of manfest file
 * generated by video publisher
 * @param subject the original string to be scaned
 * @param search  subtring to be replaced
 * @param replace substring to replace
 */
void replace_string_in_place(std::string& subject, const std::string& search,
                             const std::string& replace);

#endif
