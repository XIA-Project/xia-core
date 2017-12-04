/**
 * @file broker_server.h
 * @author Qiaoyu (Joey) Deng
 * @usage: This file contains interfaces for building a broker
 * 1. Create Xsock conections and always listen to the network.
 * 2. When client wants to play a new video, it will first set up connection with
 *    broker, and broker will generate a unique socket fd.
 * 3. With the client socket, broker will Xfork a new process to handle the new
 *    connection
 * 4. The new process will first get initial cdn list, set up the database table
 *    and then listen to client for data update
 * 5. Each time when broker get new throughput and rtt data from client, it will
 *    insert them into database in time order.
 * 6. And then analyze the historical data in the table, pick the best cdn for
 *    current request
 * 7. Keep listening until client close connection, (but this actually does not
 *    happen because of a limitation of proxy implementation)
 */


#include <stdarg.h>
#include <poll.h>
#include <signal.h>
#include <libpq-fe.h>                   // for postgresql

#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cerrno>
#include <sys/wait.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstring>
#include <utility>
#include <vector>
#include <sstream>                      // for std::stringstream convert string to size_t

#include "Xkeys.h"                      // XIA_SHA_DIGEST_STR_LEN
#include "Xsocket.h"
#include "dagaddr.hpp"                  // Graph
#include "utils.h"                      // say, die

#define BROKER_NAME "www.broker.xia"
#define LINE_END_MARK '@'
#define PROPERTIES_COUNT_TO_UPDATE 3
#define END_MARK "#END#"
#define END_LEN (strlen(END_MARK))

// epsilon cdn selection scheme
// First we try n CDNs for the first n trunks.
// For the remaining total - n chunks,
// we choose the best one from the historical data for a probability of (1-epsilon),
// but with probability of epsilon(0.2), we choose a random CDN
#define EPSILON 0.2

#define MAX_BUF_SIZE 62000
#define MAX_QUERY_SIZE 10240            // the max length of sql query

// use it to log into PostgreSQL
#define DB_CONN_INFO "user=qdeng password=61333777 dbname=testdb"

/**
 * Called by Xforked new process to handle a new connection from client
 * @param csock the socket fd to client
 */
void process(int csock);

/**
 * Get the request content from client when the connection is first set up
 * @param buf     buffer that is used to receive the data
 * @param max_len the max length of the buffer
 * @param csock   the socket fd to client
 */
void get_req_info(char *buf, size_t max_len, int csock);

/**
 * get client's name from client reply
 * @param  buf        buffer that contains client's name
 * @param  offset     the offset where search function begins
 * @param  client_dag buffer that is used to receive the client name
 * @return            1 as success, -1 as failure
 */
int get_client_dag(char *buf, size_t offset, char *client_dag);

/**
 * get the number of cdns from the list, and store it into *cdn_dag_num_ptr
 * @param  buf             buffer that contains client's name
 * @param  offset          the offset where search function begins
 * @param  cdn_dag_num_ptr the pointer to cdn_dag_num, thus we can change it
 * @return                 1 as success, -1 as failure
 */
int get_cdn_dag_num(char *buf, size_t offset, size_t *cdn_dag_num_ptr);

/**
 * parse cdn list from the buffer
 * @param buf         buffer that contains cdns
 * @param offset      the offset where search function begins
 * @param cdn_dag_num the total number of cdns
 * @param cdn_list    list that needs to be filled with cdn names
 */
void get_cdn_list(char *buf, size_t offset, size_t cdn_dag_num,
                  std::vector<std::string> &cdn_list);

/**
 * update throughput and rtt for request from client "csock", and generates new
 * cdn schedule
 * @param csock      socket fd to client
 * @param conn       database connection handler to PostgreSQL
 * @param client_dag the name of client
 * @param req_id     request id returned by database
 */
void update_and_schedule(int csock, PGconn *conn, char *client_dag, size_t req_id);

/**
 * Get data updates from client
 * @param  buf   buffer that will be used to store historical data
 * @param  csock socket fd to client
 * @return       1 as success, -1 as failure
 */
int get_updates(char *buf, int csock);

/**
 * parse data updates from client to througput, rtt
 * @param buf        buffer that contains updated data
 * @param cdn_dag    the name of cdn
 * @param throughput pointer to where throughput data should be filled
 * @param rtt        pointer to where rtt data should be filled
 */
void parse_updates(char *buf, char *cdn_dag, double *throughput, double *rtt);

/**
 * find the index of ch that first appears in the str
 * @param  str    string to be searched
 * @param  offset offset where search begins
 * @param  ch     character to search
 * @return        the index if ch in str
 */
int find_first(char *str, size_t offset, char ch);


/**
 * connect to local database
 * @return the handler of database connection
 */
PGconn *connect_db();

/**
 * close the handler of database connection
 * @param conn the handler of database connection
 */
void close_db(PGconn *conn);

/**
 * set up database table, and clear all the previous data
 * (see comments inside function)
 */
void set_up_db();

/**
 * create a new request entry into database, and initialize a new connection
 * @param  conn       database connection handler
 * @param  client_dag the name of client
 * @param  cdn_list   cdn options
 * @return            request id returned by database
 */
size_t add_new_req(PGconn *conn, char *client_dag, std::vector<std::string> &cdn_list);

/**
 * insert a new throuput and rtt record into database for current request
 * @param conn       database connection handler
 * @param client_dag the name of client for the record
 * @param cdn_dag    the name of cdn for the record
 * @param throughput throughput to insert
 * @param rtt        rtt to insert
 */
void insert_new_record(PGconn *conn, char *client_dag, char *cdn_dag,
                       double throughput, double rtt);

/**
 * pick the best for current request after analyzing historical data
 * @param conn        database connection handler
 * @param cdn_to_pick the pointer to where the chosen cdn should be stored
 * @param req_id      request id of current request
 */
void pick_cdn(PGconn *conn, char *cdn_to_pick, size_t req_id);

/**
 * epsilon greedy algothrim to pick the best cdn
 * @param conn        database connection handler
 * @param req_id      request id of current request
 * @param cdn_to_pick the pointer to where the chosen cdn should be stored
 */
void epsilon_greedy(PGconn *conn, size_t req_id, char *cdn_to_pick);

/**
 * delete all the entry for current request, including the historical data
 * @param conn   database connection handler
 * @param req_id request id of current request
 */
void release_resource(PGconn *conn, size_t req_id);

/**
 * exit the broker and release all the resources
 * @param conn     database connection handler
 * @param res      the pointer to database result
 * @param is_error 1 if call do_exit because of error occurs, otherwise 0
 */
void do_exit(PGconn *conn, PGresult *res, int is_error = 1);
