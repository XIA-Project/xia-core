/**
 * broker_client.h
 * @author Qiaoyu (Joey) Deng
 * @date 2017.11.25
 * @usage
 * 1. Each time when client wants to play a new video, it first gets the manifest
 *    file from content provider(or manifest server)
 * 2. Client sets connection with broker, and send its name and cdn options for
 *    requested video to broker, connection setup is completed
 * 3. Client randomly chooses a initial cdn, and fetch chunk from the chosen cdn
 * 4. Client calculates the throughput and rtt for this chunk and corresponding
 *    cdn, send them to broker, and update the cdn result returned from broker
 * 5. Keep this process until all the chunks have been fetched
 * 6. During the process of fetching video chunks, client cannot choose cdn itself
 *    but listen to broker's schedule
 */

#ifndef __BROKER_CLIENT_H__
#define __BROKER_CLIENT_H__
#include <stdarg.h>
#include <poll.h>

#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstring>
#include <cassert>                      // assert()

#include "Xsocket.h"
#include "utils.h"                      // say, die

#define BROKER_NAME "www_s.broker.xia"  // client uses this name to look up for the broker in name server
#define END_MARK "#END#"                // client and broker use it to mark as the end of content
#define LINE_END_MARK '@'
#define END_LEN (strlen(END_MARK))

#define MAX_BUF_SIZE 62000

/**
 * gets the addrinfo of broker from nameserver given broker_name, addrinfo can
 * then be used to set up connection with broker when client wants to request
 * new video
 * @param broker_name            char array that has the name of broker
 * @param ai_broker_ptr_location pointer to the pointer of addrinfo of broker
 * @param sa_broker_ptr_location pointer to the pointer of sockaddr_x of broker
 */
void lookup_for_broker(char *broker_name,
                       struct addrinfo **ai_broker_ptr_location,
                       sockaddr_x **sa_broker_ptr_location);

/**
 * sets up connection with broker, given sa_broker arguments, before calling this
 * function lookup_for_broker must first be called
 * @param  sa_broker the pointer of sockaddr_x of broker
 * @return           socket fd to broker
 */
int set_conn_to_broker(const sockaddr_x *sa_broker);

/**
 * initializes connection with broker, and tells the broker about current available
 * cdn options, that broker can then use to schedule optimized cdn selection
 * @param cdn_lists   list of cdn name that can be used to fetch video chunks
 * @param client_name the name of client that is going to play the video
 * @param broker_sock socket fd returned by set_conn_to_broker
 */
void init_conn(std::vector<std::string> cdn_lists,
               const char *client_name, const int broker_sock);

/**
 * get the first cdn choice from cdn options randomly
 * @param  cdn_lists list of cdn name that can be used to fetch video chunks
 * @return           the choosen cdn
 */
std::string get_init_cdn(std::vector<std::string> cdn_lists);

/**
 * report current throughput and rtt to broker, get update the cdn choice from
 * the result returned by broker
 * @param throughput  the throughput of fetching last chunk from cdn_choosed
 * @param rtt         the rtt of fetching last chunk from cdn_choosed
 * @param cdn_choosed cdn that is used to fetch last chunk
 * @param cdn         the pointer to cdn choice, which will be used to update cdn
 * @param broker_sock socket fd returned by set_conn_to_broker
 * @param mutex_ptr   the pointer to a mutex that protects cdn choice
 */
void report(const double throughput, const double rtt, const char *cdn_choosed,
            std::string *cdn, const int broker_sock, std::mutex *mutex_ptr);

/**
 * private function used by interface above
 */
/**
 * send cdn options and client's name to broker to initialize connection with
 * broker
 * @param cdn_lists   list of cdn name that can be used to fetch video chunks
 * @param client_name the name of client that is going to play the video
 * @param broker_sock socket fd returned by set_conn_to_broker
 */
void send_req_info(std::vector<std::string> cdn_lists,
                   const char *client_name, const int broker_sock);

/**
 * hand shake with broker to comfirm the connection initialization
 * @param broker_sock socket fd returned by set_conn_to_broker
 */
void wait_for_confirm(const int broker_sock);

#endif // __BROKER_CLIENT_H__