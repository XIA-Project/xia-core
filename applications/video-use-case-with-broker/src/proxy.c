#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cctype>
#include <time.h>
#include <stdarg.h>
#include <iostream>
#include <pthread.h>
#include <sstream>                      // std::sstream
#include <unordered_map>                // std::unordered_map
#include <algorithm>                    // std::replace

#include "proxy.h"
#include "broker_client.h"

static int port = 0;                    // port this server runs on
static int list_s;                      // listening socket of this proxy
static int reuseaddr = 1;               // need to reuse the address for binding
static int client_sockets[MAX_CLIENTS]; // all client sockets
static XcacheHandle xcache;             // xcache instance

unsigned int numthreads = 0;            // Current number of clients
pthread_mutex_t numthreadslock;

// broker's address info
struct addrinfo *ai_broker_g;
sockaddr_x *sa_broker_g;

// request id counter
size_t req_id = 0;
std::mutex req_id_mutex;

// request id to ctx
std::unordered_map<size_t, ProxyRequestCtx *> req_id_to_ctx;
std::mutex map_mutex;

// the lookup cache for CDN nameserver query
//  CDN name -> server location (AD:HID)
static map<string, vector<string> > dagLookupCache;
// the map of CDN name to CDN properties
//  name -> CDN properties
static map<string, CDN> cdns;
// map of origin name to the picked CDN
static map<string, string> originCDNCache;

void usage() {
    say("usage: ./proxy <port>\n");
}

void cleanup(int sig) {
    UNUSED(sig);

    // try to close the listening socket
    if (close(list_s) < 0) {
        fprintf(stderr, "Error calling close()\n");
        exit(EXIT_FAILURE);
    }

    // exit with success
    exit(EXIT_SUCCESS);
}

void close_fd(int browser_sock) {
    if (browser_sock >= 0) {
        Close(browser_sock);
        say("browser socket is closed successfully\n");
    }
}

/**
 * before calling this function, host_port has host:port info, host_port is
 * parsed into: abc.com:15641
 * remote_host: abc.com
 * remote_port: 15641 or 80 if no port number contained
 * @param host_port
 * @param remote_host
 * @param remote_port
 */
void parse_host_port(char *host_port, char *remote_host, char *remote_port) {
    char *tmp = NULL;
    tmp = index(host_port, ':');
    if (tmp != NULL) {
        *tmp = '\0';
        strcpy(remote_port, tmp + 1);
    } else {
        strcpy(remote_port, "80");
    }
    strcpy(remote_host, host_port);
}

/**
 * convert plain text dag string to sockaddr_x dag
 * @param dagUrls
 * @param chunkAddresses sockaddr_x dag
 */
void process_urls_to_DAG(vector<string> & dagUrls, sockaddr_x* chunkAddresses) {
    for (unsigned i = 0; i < dagUrls.size(); ++i) {
        // dagUrl:
        // http://DAG.3.0.-.AD$c26124c5c3c3a964c03693c4bdfb625bb21faa1f.1.-.
        // HID$117a77f63181b5d6e63d5288d00cc09131462df5.2.-.
        // SID$15b7347ac78fc9aded44fbca0e6243698144fedd.3.-.
        // CID$7f521ad37897913c2f28cc21b79e098f9642073e
        string dagUrl = dagUrls[i];

        size_t found = dagUrl.find("http://");
        if (found == string::npos) {
            dagUrl = "http://" + capitalize_XID(dagUrl);
        }

        Graph parsed(dagUrl);
        parsed.fill_sockaddr(&chunkAddresses[i]);
    }
}

void *job(void *sockptr) {
    int rc;
    int browser_sock = *((int *)sockptr);

    rc = xia_proxy_handle_request(browser_sock);
    if (rc == -1) {
        warn("warning: something wrong with the xia_proxy_handle_request, close the browser socket\n");
    }

    close_fd(browser_sock);

    if (pthread_mutex_lock(&numthreadslock)) {
        perror("proxy: ERROR: locking numthreads variable");
        return NULL;
    }
    if (numthreads > 0) {
        numthreads--;
    }
    if (pthread_mutex_unlock(&numthreadslock)) {
        perror("proxy: ERROR: unlocking numthreads variable");
        return NULL;
    }
    return NULL;
}

int send_command(ProxyRequestCtx *ctx, const char *cmd) {
    int n;
    // cmd: xhttp://www.origin.xia/dash1.mpd
    if ((n = Xsend(ctx->xia_sock, cmd, strlen(cmd), 0)) < 0) {
        Xclose(ctx->xia_sock);
        warn("Unable to communicate\n");
        return -1;
    }
    return 1;
}

int get_server_reply(ProxyRequestCtx *ctx, char *reply, int sz) {
    int n = -1;
    if ((n = Xrecv(ctx->xia_sock, reply, sz, 0))  < 0) {
        Xclose(ctx->xia_sock);
        warn("Unable to communicate with the server\n");
        return -1;
    }

    reply[n] = 0;

    return n;
}

int send_and_receive_reply(ProxyRequestCtx *ctx, char* cmd, char* reply) {
    int status = send_command(ctx, cmd);

    if (get_server_reply(ctx, reply, XIA_MAXBUF) < 1) {
        warn("could not get chunk count. Aborting. \n");
        return -1;
    }
    return status;
}

int get_xia_socket_for_request(const char* sname) {
    // sname: www.origin.xia
    int xia_sock;
    sockaddr_x dag;
    socklen_t daglen;

    daglen = sizeof(dag);

    // get the service DAG associated with the CDN service name
    if (XgetDAGbyName(sname, &dag, &daglen) < 0) {
        warn("unable to locate CDN DNS service name: %s\n", sname);
        return -1;
    }

    if ((xia_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
        warn("Unable to create the listening socket\n");
        return -1;
    }

    if (Xconnect(xia_sock, (struct sockaddr*)&dag, daglen) < 0) {
        Xclose(xia_sock);
        warn("Unable to bind to the dag: %s\n", dag);
        return -1;
    }

    return xia_sock;
}

int handle_cross_origin_probe(ProxyRequestCtx *ctx) {
    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_status_ok, strlen(http_chunk_header_status_ok)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_same_origin, strlen(http_chunk_header_same_origin)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    if (Rio_writen(ctx->browser_sock, (char*)http_header_allow_headers, strlen(http_header_allow_headers)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    if (Rio_writen(ctx->browser_sock, (char*)http_header_allow_methods, strlen(http_header_allow_methods)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_end_marker, strlen(http_chunk_header_end_marker)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    return 1;
}

int handle_manifest_requests(ProxyRequestCtx *ctx) {
    char cmd[XIA_MAXBUF];
    char reply[XIA_MAXBUF];

    // send the request for manifest
    // remote_host: www.origin.xia
    // remote_path: /dash1.mpd
    // cmd: xhttp://www.origin.xia/dash1.mpd
    sprintf(cmd, "xhttp://%s%s", ctx->remote_host, ctx->remote_path);
    memset(reply, '\0', sizeof(reply));
    if (send_and_receive_reply(ctx, cmd, reply) < 0) {
        return -1;
    }
    // reply:
    // http://DAG.3.0.-.AD$c26124c5c3c3a964c03693c4bdfb625bb21faa1f.1.-.
    // HID$117a77f63181b5d6e63d5288d00cc09131462df5.2.-.
    // SID$15b7347ac78fc9aded44fbca0e6243698144fedd.3.-.
    // CID$7f521ad37897913c2f28cc21b79e098f9642073e
    // (space)
    // (2nd url)
    vector<string> dagUrls = split_string_on_delimiter(reply, " ");
    int numChunks = dagUrls.size();
    sockaddr_x chunkAddresses[numChunks];
    process_urls_to_DAG(dagUrls, chunkAddresses);

    // header is irrelavent to the actual content, so we can forward hader
    // first.
    if (forward_http_header_to_client(ctx, CONTENT_MANIFEST) < 0) {
        warn("unable to forward manifest to client\n");
        return -1;
    }


    if (forward_chunks_to_client(ctx, chunkAddresses, numChunks, false) < 0) {
        warn("unable to forward chunks to client\n");
        return -1;
    }

    return 1;
}

int handle_stream_requests(ProxyRequestCtx *ctx) {
    string cname;
    vector<string> dagUrls;

    if (strstr(ctx->remote_host, XIA_DAG_URL) != NULL) {
        // url sent by client contains all the routing info, direct fetch chunk
        dagUrls = split_string_on_delimiter(ctx->remote_host, " ");
    } else if (strstr(ctx->remote_path, "/CID") != NULL) {
        // if the current url has name with CDN
        // if it is cdn case, ctx->remote_host: www.cdn1.xia
        // if it is multi cdn case, ctx->remote_host: www.origin.xia
        if (strstr(ctx->remote_host, XIA_CDN_SERVICE) != NULL) {
            // ctx->remote_path + 1 jumps over the /, go to resource path directly
            dagUrls = cdn_name_to_dag_urls(ctx->remote_host, ctx->remote_path + 1);
        }
        // if the current url says this is multi-cdn use case,
        // pick a CDN name and get the server DAG in the CDN.
        else if (strstr(ctx->remote_host, XIA_VID_SERVICE) != NULL) {
            string pname = ctx->remote_path + 1;
            size_t start = pname.find("CID:");
            size_t end = pname.find("?");

            size_t len = end - start;
            if (len <= 4) {
                return -1;
            }

            string cid = pname.substr(start, len);
            string cdn_options = pname.substr(end + 1);
            ctx->cdn_mutex->lock();
            cname = *ctx->cdn;
            ctx->cdn_mutex->unlock();
            // cname = multicdn_name_to_CDN_name(ctx->remote_host, (char*)cdn_options.c_str());
            // modify here! cname contains cdn to be picked
            dagUrls = cdn_name_to_dag_urls((char*)cname.c_str(), (char*)cid.c_str());
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    int numChunks = dagUrls.size();
    sockaddr_x chunkAddresses[numChunks];
    process_urls_to_DAG(dagUrls, chunkAddresses);

    if (forward_http_header_to_client(ctx, CONTENT_STREAM) < 0) {
        warn("unable to forward manifest to client\n");
        return -1;
    }

    if (forward_chunks_to_client(ctx, chunkAddresses, numChunks, true, cname.c_str()) < 0) {
        warn("unable to forward chunks to client\n");
        return -1;
    }

    std::cout << "proxy pick CDN: " << cname << ", host within CDN: " << dagUrls[0] << std::endl;

    for (auto it = cdns.begin(); it != cdns.end(); it++) {
        std::cout << "CDN name: " << it->first << std::endl;
        std::cout << "\tCDN num requests: " << it->second.num_reqs << std::endl;
        std::cout << "\tCDN average throughput: " << it->second.avg_throughput << std::endl;
    }

    return 1;
}

int xia_proxy_handle_request(int browser_sock) {
    int n;
    char buf[MAXLINE], nxtBuf[MAXLINE];
    char method[MAXLINE], protocol[MAXLINE];
    char host_port[MAXLINE];
    char remote_host[MAXLINE], remote_port[MAXLINE], resource[MAXLINE];
    char version[MAXLINE];

    rio_t rio_client;
    strcpy(remote_host, "");
    strcpy(remote_port, "80");

    // read the first line of HTTP request
    Rio_readinitb(&rio_client, browser_sock);

    n = Rio_readlineb(&rio_client, buf, MAXLINE);
    if (n == -1) {
        warn("problem with reading from the socket\n");
        return -1;
    } else if (n == 0) {
        // FIXME memory leak here!
        // stateless connection makes it hard to determine whether the current
        // connection should be cleared
        // thins needs to be cleared:
        // ProxyRequestCtx *ctx
        // ctx->mutex
        // ctx->cdn
        // Xclose(ctx->broker);
        return 1;
    }

    // finish reading the rest of the HTTP request
    // need this since socket buffer for this request
    // need to be completly drained
    // since we only need the first line to figure out the video name
    // we just pass the parameter part
    while (Rio_readlineb(&rio_client, nxtBuf, MAXLINE) != 0) {
        if (strcmp(nxtBuf, "\r\n") == 0) {
            break;
        }
    }

    // request line's format:
    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    // GET http://abc.com:15641/resources HTTP/1.1
    // method: GET
    // protocol: http
    // host_port: abc.com:15641
    // resources: /resources
    if (parse_request_line(buf, method, protocol, host_port,
                           resource, version) == -1) {
        return -1;
    }

    // remote host: abc.com
    // remote port: 15641
    parse_host_port(host_port, remote_host, remote_port);

    // check whether it contains valid url
    if (strcmp(remote_host, "") == 0 || (strstr(remote_host, XIA_VID_SERVICE) == NULL && strstr(remote_host, XIA_DAG_URL) == NULL)) {
        warn("[Proxy] service id not XIA type %s\n", remote_host);
        return -1;
    }

    if (strstr(method, "GET") != NULL || strstr(method, "OPTIONS")) {
        ProxyRequestCtx *ctx = get_ctx(remote_host);

        ctx->browser_sock = browser_sock;
        strcpy(ctx->remote_host, remote_host);
        strcpy(ctx->remote_port, remote_port);
        strcpy(ctx->remote_path, resource);
        if (ctx->cdn_mutex == nullptr) {
            ctx->cdn_mutex = new std::mutex();
            ctx->cdn = new std::string();
        }

        if (strstr(ctx->remote_host, XIA_DAG_URL) != NULL || strstr(ctx->remote_path, "/CID") != NULL) {
            if (handle_stream_requests(ctx) < 0) {
                warn("failed to return back chunks to browser. Exit\n");
                return -1;
            }
        } else if (strstr(ctx->remote_host, XIA_VID_SERVICE) != NULL) {
            // if this is option probe,
            if (strstr(method, "OPTIONS") != NULL) {
                if (handle_cross_origin_probe(ctx) < 0) {
                    warn("failed to handle cross origin probe back to browser. Exit\n");
                    return -1;
                }
            } else {
                // manifest request must request .mpd files as extension
                if (strstr(ctx->remote_path, ".mpd") == NULL) {
                    warn("request remote path not mpd manifest type\n");
                    return -1;
                }

                // get the XIA socket to the video server
                int xia_sock = get_xia_socket_for_request(remote_host);
                if (xia_sock < 0) {
                    warn("failed to create socket with the video server. Exit\n");
                    return -1;
                }
                ctx->xia_sock = xia_sock;

                if (handle_manifest_requests(ctx) < 0) {
                    warn("failed to return back chunks to browser. Exit\n");
                    return -1;
                }
            }
        }

        return 0;
    } else {
        warn("unsupported request method %s for %s\n", method, host_port);
        return -1;
    }
}

int forward_chunks_to_client(ProxyRequestCtx *ctx, sockaddr_x* chunkAddresses, int numChunks, bool cdn, const char *cdn_choosed) {
    int len = -1, totalBytes = 0;
    double elapsedTime;
    char *data = NULL;
    struct timeval t1, t2;

    if (cdn) {
        for (int i = 0; i < numChunks; i++) {
            gettimeofday(&t1, NULL);
            if ((len = XfetchChunk(&xcache, (void**)&data, XCF_BLOCK, &chunkAddresses[i], sizeof(chunkAddresses[i]))) < 0) {
                die(-1, "XcacheGetChunk Failed\n");
            }
            gettimeofday(&t2, NULL);

            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms

            double curr_throughput = len / elapsedTime;
            double rtt = elapsedTime;
            std::thread t(report, curr_throughput, rtt, cdn_choosed,
                          ctx->cdn, ctx->broker_sock, ctx->cdn_mutex);
            t.detach();

            say("size of data: %d\n", len);
            say("elapsed time for xfetch: %f\n", elapsedTime);

            totalBytes += len;

            gettimeofday(&t1, NULL);
            // send to browser socket here. Once we reach here, we know it would be success
            if (forward_http_response_body_to_client(ctx, data, len) < 0) {
                die(-1, "error when sending response body to the browser\n");
            }
            gettimeofday(&t2, NULL);
            elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
            elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
            say("elapsed time for forwarding: %f\n", elapsedTime);
        }
    } else {
        // manifest request
        std::string manifest_file;
        std::vector<int> chunk_lens;
        chunk_lens.push_back(0);
        for (int i = 0; i < numChunks; i++) {
            if ((len = XfetchChunk(&xcache, (void**)&data, XCF_BLOCK, &chunkAddresses[i], sizeof(chunkAddresses[i]))) < 0) {
                die(-1, "XcacheGetChunk Failed\n");
            }
            manifest_file.append(data, len);
            chunk_lens.push_back(len);
        }

        size_t curr_req_id = 0;
        req_id_mutex.lock();
        curr_req_id = req_id++;
        req_id_mutex.unlock();
        // strlen("@XXXXXXXXXX@") == 12
        char req_id_place[13];
        snprintf(req_id_place, 13, "@%0*zu@", 10, curr_req_id);
        assert(strlen(req_id_place) == 12);
        // std::replace(manifest_file.begin(), manifest_file.end(),
        //              "@XXXXXXXXXX@", (const char*)req_id_place);
        replace_string_in_place(manifest_file,
                                std::string("@XXXXXXXXXX@"),
                                std::string(req_id_place));

        std::vector<string> cdn_lists;
        get_cdn_list(manifest_file.c_str(), cdn_lists);
        ctx->broker_sock = set_conn_to_broker(sa_broker_g);
        {
            char client_ip[20] = {0};
            struct sockaddr_in addr;
            socklen_t addr_size = sizeof(struct sockaddr_in);
            getpeername(ctx->browser_sock,
                        (struct sockaddr *)&addr, &addr_size);
            strcpy(client_ip, inet_ntoa(addr.sin_addr));
            std::string client_name_str(client_ip);
            ctx->client_name = client_name_str;
        }
        init_conn(cdn_lists, ctx->client_name.c_str(), ctx->broker_sock);
        *ctx->cdn = get_init_cdn(cdn_lists);

        map_mutex.lock();
        req_id_to_ctx[curr_req_id] = ctx;
        map_mutex.unlock();

        for (int i = 1; i < chunk_lens.size(); i++) {
            char *offset = (char *)manifest_file.c_str() + chunk_lens[i - 1];
            if (forward_http_response_body_to_client(ctx, offset, chunk_lens[i]) < 0) {
                die(-1, "error when sending response body to the browser\n");
            }
        }
    }

    if (data) {
        free(data);
    }
    return totalBytes;
}

int forward_http_header_to_client(ProxyRequestCtx *ctx, int type) {
    // forward status line (should be OK if chunk is retrieved correctly)
    // "HTTP/1.0 200 OK\r\n"
    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_status_ok, strlen(http_chunk_header_status_ok)) == -1) {
        warn("unable to forward the http status ok\n");
        return -1;
    }

    // forward the Date field
    char http_chunk_header_date[MAXLINE];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    // "Date: Mon, 24 Oct, 2017 23:59:59 +8\r\n"
    strftime(http_chunk_header_date, sizeof(http_chunk_header_date), http_chunk_header_date_fmt, &tm);
    if (Rio_writen(ctx->browser_sock, http_chunk_header_date, strlen(http_chunk_header_date)) == -1) {
        warn("unable to forward the http header date\n");
        return -1;
    }

    // forward the content-type field
    //  "Content-Type: application\r\n"
    if (type == CONTENT_MANIFEST) {
        if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_mpd_content_type, strlen(http_chunk_header_mpd_content_type)) == -1) {
            warn("unable to forward the http header content type\n");
            return -1;
        }
    } else if (type == CONTENT_STREAM) {
        if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_mp4_content_type, strlen(http_chunk_header_mp4_content_type)) == -1) {
            warn("unable to forward the http header content type\n");
            return -1;
        }
    } else {
        warn("unknown content type!! \n");
        return -1;
    }

    // forward the connection field
    if (Rio_writen(ctx->browser_sock, (char*)connection_str, strlen(connection_str)) == -1) {
        warn("unable to forward the http status line\n");
        return -1;
    }

    // forward the server field
    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_server, strlen(http_chunk_header_server)) == -1) {
        warn("unable to forward the http status line\n");
        return -1;
    }

    // forward allowing same origin
    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_same_origin, strlen(http_chunk_header_same_origin)) == -1) {
        warn("unable to forward the http same origin line\n");
        return -1;
    }

    // finally terminate with \r\n
    // "\r\n"
    if (Rio_writen(ctx->browser_sock, (char*)http_chunk_header_end_marker, strlen(http_chunk_header_end_marker)) == -1) {
        warn("unable to forward the http status line\n");
        return -1;
    }

    return 0;
}

int forward_http_response_body_to_client(ProxyRequestCtx *ctx, char* data, int len) {
    if (Rio_writen(ctx->browser_sock, data, len) < 0) {
        warn("problem with rio write when write data\n");
        return -1;
    }

    return 0;
}

int parse_request_line(char *buf, char *method, char *protocol,
                       char *host_port, char *resource, char *version) {
    char url[MAXLINE];
    // check if it is valid buffer
    if (strstr(buf, "/") == NULL || strlen(buf) < 1) {
        return -1;
    }
    // set resource default to '/'
    strcpy(resource, "/");
    // GET http://abc.com:15641/resources HTTP/1.1
    // method: GET
    // protocol: http
    // host_port: abc.com:15641
    // resources: /resources
    sscanf(buf, "%s %s %s", method, url, version);
    if (strstr(url, "://") != NULL) {
        // has protocol
        sscanf(url, "%[^:]://%[^/]%s", protocol, host_port, resource);
    } else {
        // no protocols
        sscanf(url, "%[^/]%s", host_port, resource);
    }
    return 0;
}

int main(int argc, char const *argv[]) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int new_socket, i;
    struct sockaddr_in servaddr; //  socket address structure

    // set up signal handler for ctrl-c
    // clean up closes listen sock
    (void) signal(SIGINT, cleanup);

    // write on closed pipe (socket)
    (void) signal (SIGPIPE, SIG_IGN);

    if (argc != 2) {
        printf("Must specify a port number and whether to use connection close\n");
        return -1;
    }
    port = atoi(argv[1]);

    // create the listening socket
    if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf(stderr, "Error creating listening socket.\n");
        exit(EXIT_FAILURE);
    }

    /* Enable the socket to reuse the address */
    // set listen socket's SO_REUSEADDR option to reuseaddr(1, which means enable it)
    if (setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
        printf("Let us reuse the address on the socket\n");
        exit(EXIT_FAILURE);
    }

    // set all bytes in socket address structure to zero, and fill in the relevant data members
    // set my to be bound ip address to my default address without knowing it first
    // and port is set to the specified one
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    // bind to the socket address
    if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
        fprintf(stderr, "Error calling bind()\n");
        exit(EXIT_FAILURE);
    }

    // Listen on socket list_s
    if ( (listen(list_s, 10)) == -1) {
        fprintf(stderr, "Error Listening\n");
        exit(EXIT_FAILURE);
    }

    printf("Listen on the socket\n");

    for (i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // initilize xcache
    XcacheHandleInit(&xcache);

    // Initialize a mutex to guard numthreads global
    if (pthread_mutex_init(&numthreadslock, NULL)) {
        perror("proxy: ERROR initializing lock for numthreads");
        return -1;
    }

    lookup_for_broker(BROKER_NAME, &ai_broker_g, &sa_broker_g);
    while (1) {
        // Accept incoming connection requests from clients
        new_socket = accept(list_s, (struct sockaddr *)&address,
                            (socklen_t *) &addrlen);
        if (new_socket < 0) {
            perror("proxy: ERROR: accept failed");
            continue;
        }

        // Check if we can create another thread
        if (pthread_mutex_lock(&numthreadslock)) {
            perror("proxy: ERROR: locking numthreads variable");
            return -1;
        }
        if (numthreads++ < MAX_CLIENTS) {
            // Create a new thread
            pthread_t worker;
            if (pthread_create(&worker, NULL, job, (void *)&new_socket)) {
                printf("proxy: ERROR: creating handler. Dropping request\n");
            }
        }
        if (pthread_mutex_unlock(&numthreadslock)) {
            perror("proxy: ERROR: unlocking numthreads variable");
            return -1;
        }
    }

    close(list_s);
    return 0;
}

string capitalize_XID(string dagUrl) {
    // need to capitalize dag, ad, hid, cid
    size_t found = dagUrl.find(XIA_DAG_URL);
    for (int i = 0; i < 3; ++i) {
        dagUrl[i + found] = toupper(dagUrl[i + found]);
    }

    found = dagUrl.find(XIA_AD);
    for (int i = 0; i < 2; ++i) {
        dagUrl[i + found] = toupper(dagUrl[i + found]);
    }

    found = dagUrl.find(XIA_HID);
    for (int i = 0; i < 3; ++i) {
        dagUrl[i + found] = toupper(dagUrl[i + found]);
    }

    found = dagUrl.find(XIA_CID);
    for (int i = 0; i < 3; ++i) {
        dagUrl[i + found] = toupper(dagUrl[i + found]);
    }

    found = dagUrl.find(XIA_SID);
    for (int i = 0; i < 3; ++i) {
        dagUrl[i + found] = toupper(dagUrl[i + found]);
    }

    return dagUrl;
}

Graph cid2addr(std::string CID, std::string AD, std::string HID) {
    Node n_src;
    Node n_cid(XID_TYPE_CID, strchr(CID.c_str(), ':') + 1);
    Node n_ad(XID_TYPE_AD, strchr(AD.c_str(), ':') + 1);
    Node n_hid(XID_TYPE_HID, strchr(HID.c_str(), ':') + 1);

    Graph primaryIntent = n_src * n_cid;
    Graph gFallback = n_src * n_ad * n_hid * n_cid;
    Graph gAddr = primaryIntent + gFallback;

    return gAddr;
}

/**
 * convert cdn options to actual dag urls that can be used to form sockaddr_x
 */
vector<string> cdn_name_to_dag_urls(char* sname, char* cidString) {
    sockaddr_x dag;
    socklen_t daglen = sizeof(dag);
    string sname_str = sname;
    string cid_str = cidString;
    vector<string> result;

    if (!sname || *sname == 0) {
        return result;
    }

    // if the CDN name lookup is in cache locally, don't do the resolution
    if (dagLookupCache.find(sname_str) == dagLookupCache.end()) {
        // get the DNS DAG associated with the CDN service name
        if (XgetDAGbyAnycastName(sname, &dag, &daglen) < 0) {
            warn("unable to locate CDN DNS service name: %s\n", sname);
            return result;
        }

        // construct the dag for CID using HID and AD
        Graph g;
        g.from_sockaddr(&dag);

        // find out the AD and HID for the DAG and construct the full DAG
        // for CID
        string AD, HID;
        for (int i = 0; i < g.num_nodes(); i++) {
            Node currNode = g.get_node(i);

            if (currNode.type_string() == Node::XID_TYPE_AD_STRING) {
                AD = currNode.to_string();
            } else if (currNode.type_string() == Node::XID_TYPE_HID_STRING) {
                HID = currNode.to_string();
            }
        }

        Graph g2 = cid2addr(cid_str, AD, HID);
        result.push_back(g2.http_url_string());

        dagLookupCache[sname_str].push_back(AD);
        dagLookupCache[sname_str].push_back(HID);

        return result;
    } else {
        string AD = dagLookupCache[sname_str][0];
        string HID = dagLookupCache[sname_str][1];

        Graph g2 = cid2addr(cid_str, AD, HID);
        result.push_back(g2.http_url_string());

        return result;
    }
}

string multicdn_select_cdn_strategy(string origin, const vector<string> & options) {
    // origin: cdn1=www.cdn1.xia&amp;cdn2=www.cdn2.xia
    // if we haven't seen a CDN, put it in our accounting
    for (unsigned i = 0; i < options.size(); i++) {
        if (cdns.find(options[i]) == cdns.end()) {
            CDN c = {0, 0.0};
            cdns[options[i]] = c;
        }
    }

    // select a CDN if the video request it has seen is less than a threshold
    string c_max_s;
    double c_max = -DBL_MAX;
    for (unsigned i = 0; i < options.size(); i++) {
        if (cdns[options[i]].num_reqs < BOOTSTRAP_NUM_REQS) {
            // change the mapping
            originCDNCache[origin] = options[i];
            return options[i];
        } else {
            if (cdns[options[i]].avg_throughput > c_max) {
                c_max_s = options[i];
                c_max = cdns[options[i]].avg_throughput;
            }
        }
    }

    // if we need to repick a CDN, select the one with the largest
    // throughput
    if (originCDNCache.find(origin) == originCDNCache.end()) {
        originCDNCache[origin] = c_max_s;
    }

    return originCDNCache[origin];
}

string multicdn_name_to_CDN_name(char* origin, char* options) {
    // & instead of &amp;?
    // origin: cdn1=www.cdn1.xia&amp;cdn2=www.cdn2.xia
    vector<string> voptions = split_string_on_delimiter(options, "&");

    for (unsigned i = 0; i < voptions.size(); i++) {
        voptions[i] = voptions[i].substr(voptions[i].find("=") + 1);
    }

    return multicdn_select_cdn_strategy(origin, voptions);
}

vector<string> split_string_on_delimiter(char* str, char* delimiter) {
    vector<string> result;

    char * pch;
    pch = strtok (str, delimiter);
    while (pch != NULL) {
        result.push_back(pch);
        pch = strtok (NULL, delimiter);
    }

    return result;
}

ProxyRequestCtx *get_ctx(char *remote_host) {
    if (strstr(remote_host, "@") == nullptr) {
        ProxyRequestCtx *res = new ProxyRequestCtx;
        res->cdn_mutex = nullptr;
        return res;
    }
    size_t req_id;
    {
        char *req_id_start = strstr(remote_host, "@") + 1;
        assert(req_id_start - remote_host == 1);
        char *req_id_end = strstr(req_id_start, "@");
        size_t len = req_id_end - req_id_start;
        std::stringstream sstream(std::string(req_id_start, len));
        sstream >> req_id;

        len += 2;
        size_t i = 0;
        while (true) {
            remote_host[i] = remote_host[i + len];
            if (remote_host[i] == 0) break;
            i++;
        }
    }
    map_mutex.lock();
    assert(req_id_to_ctx.find(req_id) != req_id_to_ctx.end());
    ProxyRequestCtx *res = req_id_to_ctx[req_id];
    map_mutex.unlock();

    return res;
}

void get_cdn_list(const char *data, vector<string> &cdn_list) {
    char *start = strstr((char *)data, "<SegmentURL media=\"");
    if (start == nullptr) die(-1, "manifest file is invalid");
    char *end = strstr(start, "\"/>");
    if (end == nullptr) die(-1, "manifest file is invalid");
    // from start to end:
    // http://www.origin.xia/CID:789a634b0c47b5b8f794167b6fcbf076c7ed5eb5?cdn1=www.cdn1.xia&amp;cdn2=www.cdn2.xia
    size_t len = end - start;
    char *cdn_options_str = new char[len + 1]();
    std::memcpy(cdn_options_str, start, len);
    cdn_options_str[len] = 0;
    char *option_start = strstr(cdn_options_str, "?") + 1;
    // options:
    // cdn1=www.cdn1.xia
    // cdn2=www.cdn2.xia
    // cdn3=www.cdn3.xia
    // ...
    std::vector<string> options = split_string_on_delimiter(option_start, "&amp;");
    delete[] cdn_options_str;

    for (size_t i = 0; i < options.size(); i++) {
        std::string str = options[i];
        // str: cdn1=www.cdn1.xia
        cdn_list.push_back(str.substr(str.find('=') + 1));
    }
}

void replace_string_in_place(std::string& subject, const std::string& search,
                             const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}
