#include "broker_server.h"

/**
 * when broker exit abnormaly, reap all the child process
 * @param sig signal cached
 */
static void reaper(int sig) {
    if (sig == SIGCHLD) {
        while (waitpid(0, NULL, WNOHANG) > 0)
            ;
    }
}

int main() {
    set_up_db();
    char buf[MAX_BUF_SIZE] = {0};
    int acceptor, csock;
    char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

    say("max_dag_len: %d", XIA_MAX_DAG_STR_SIZE);
    if (signal(SIGCHLD, reaper) == SIG_ERR) {
        die(-1, "unable to catch SIGCHLD");
    }

    if ((acceptor = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
        die(-2, "unable to create the stream socket\n");

    if (XmakeNewSID(sid_string, sizeof(sid_string))) {
        die(-1, "Unable to create a temporary SID");
    }

    struct ifaddrs *ifaddr, *ifa;
    if (Xgetifaddrs(&ifaddr)) {
        die(-1, "unable to get interface addresses\n");
    }

    std::string rvdagstr;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        if (ifa->ifa_flags & XIFA_RVDAG) {
            Graph rvdag((sockaddr_x *)ifa->ifa_addr);
            rvdagstr = rvdag.dag_string();
            break;
        }
    }

    struct addrinfo hints, *ai;
    bzero(&hints, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_XIA;
    if (rvdagstr.size() > XIA_XID_STR_SIZE) {
        hints.ai_flags |= XAI_DAGHOST;
        if (Xgetaddrinfo(rvdagstr.c_str(), sid_string, &hints, &ai)) {
            die(-1, "getaddrinfo with rvdag failed\n");
        }
    } else {
        if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0) {
            die(-1, "getaddrinfo failure!\n");
        }
    }

    sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;
    xia_ntop(AF_XIA, sa, buf, sizeof(buf));
    printf("\nStream DAG\n%s\n", buf);

    if (XregisterName(BROKER_NAME, sa) < 0 )
        die(-1, "error registering name: %s\n", BROKER_NAME);
    say("registered name: \n%s\n", BROKER_NAME);

    if (Xbind(acceptor, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
        die(-3, "unable to bind to the dag\n");
    }

    // broker can accept up to 10 connections at the same
    Xlisten(acceptor, 10);

    while (1) {
        say("Xsock %4d waiting for a new connection.\n", acceptor);
        sockaddr_x sa;
        socklen_t sz = sizeof(sa);
        if ((csock = Xaccept(acceptor, (sockaddr*)&sa, &sz)) < 0) {
            warn("Xsock %d accept failure! error = %d\n", acceptor, errno);
            // FIXME: should we die here instead or try and recover?
            continue;
        }
        memset(buf, 0, sizeof(buf));
        xia_ntop(AF_XIA, &sa, buf, sizeof(buf));
        say ("Xsock %4d new session\n", csock);
        say("peer:%s\n", buf);

        // fork() does not work here because XIA protocol will break the normal
        // fork behavior
        pid_t pid = Xfork();

        if (pid == -1) {
            die(-1, "FORK FAILED\n");
        } else if (pid == 0) {
            // child
            say("I am child\n");
            Xclose(acceptor);
            process(csock);
            exit(0);
        } else {
            // parent
            Xclose(csock);
        }
        say("I am parent\n");
    }
    say("It is impossible\n");
    Xclose(acceptor);
}

void process(int csock) {
    PGconn *conn = connect_db();

    char buf[MAX_BUF_SIZE] = {0};
    get_req_info(buf, MAX_BUF_SIZE - 1, csock);

    char client_dag[MAX_BUF_SIZE] = {0};
    size_t cdn_dag_num = 0;
    int buf_offset = get_client_dag(buf, 0, client_dag) + 1;
    buf_offset = get_cdn_dag_num(buf, buf_offset, &cdn_dag_num) + 1;

    say("#client dag:#\n%s\n", client_dag);
    say("#cdn_dag_num:#\n%ld\n", cdn_dag_num);

    say("##client: %s set new connection with broker##\n", client_dag);

    say("##CDN list:##\n");
    std::vector<std::string> cdn_list;
    get_cdn_list(buf, buf_offset, cdn_dag_num, cdn_list);
    say("cdn_dag_num: %zu\n", cdn_dag_num);
    assert(cdn_list.size() == cdn_dag_num);

    // insert new request into database
    size_t req_id = add_new_req(conn, client_dag, cdn_list);
    say("sending END_MARK\n");

    int sent = Xsend(csock, END_MARK, strlen(END_MARK), 0);
    if (sent < 0) {
        die(-1, "Send error %d on socket %d\n, when confirm connection setup",
            errno, csock);
    }

    say("##waiting for cdn info updates##\n");
    update_and_schedule(csock, conn, client_dag, req_id);

    say("client %s completes request\n", client_dag);
    release_resource(conn, req_id);
    Xclose(csock);
}

void get_req_info(char *buf, size_t max_len, int csock) {
    int count = 0;
    size_t received = 0;

    while ((count = Xrecv(csock, &buf[received], max_len - received, 0)) > 0) {
        received += count;
        buf[received] = 0;
        if (received >= END_LEN
                && strncmp(buf + received - END_LEN, END_MARK, END_LEN) == 0)
            break;
    }

    if (count < 0) {
        die(-1, "Recv error %d on client socket %d when set broker connection\n",
            errno, csock);
    }
    assert(strncmp(buf + received - END_LEN, END_MARK, END_LEN) == 0);
}

int get_client_dag(char *buf, size_t offset, char *client_dag) {
    int limit = find_first(buf, offset, LINE_END_MARK);
    memcpy(client_dag, buf + offset, limit - offset);
    return limit;
}

int get_cdn_dag_num(char *buf, size_t offset, size_t *cdn_dag_num_ptr) {
    char cdn_dag_num_str[256] = {0};
    int limit = find_first(buf, offset, LINE_END_MARK);
    memcpy(cdn_dag_num_str, buf + offset, limit - offset);
    *cdn_dag_num_ptr = atoi(cdn_dag_num_str);
    return limit;
}

void get_cdn_list(char *buf, size_t offset, size_t cdn_dag_num,
                  std::vector<std::string> &cdn_list) {
    int end = 0;
    for (size_t i = 0; i < cdn_dag_num; i++) {
        char cdn_dag[MAX_BUF_SIZE] = {0};
        end = find_first(buf, offset, LINE_END_MARK);
        memcpy(cdn_dag, buf + offset, end - offset);
        say("%s\n", cdn_dag);
        std::string str(cdn_dag);
        cdn_list.push_back(str);
        memset(cdn_dag, 0, end - offset);
        end++;
        offset = end;
    }
}

void update_and_schedule(int csock, PGconn *conn, char *client_dag, size_t req_id) {
    char buf[MAX_BUF_SIZE] = {0};
    struct pollfd pfds[2];
    pfds[0].fd = csock;
    pfds[0].events = POLLIN;
    int conn_alive = 1;
    int count = 0;

    // conn_alive is set to 0 when client closes the socket
    while (conn_alive) {
        if ((count = Xpoll(pfds, 1, -1)) <= 0) {
            die(-5, "Poll returned %d\n", count);
        }
        conn_alive = get_updates(buf, csock);
        if (!conn_alive) break;

        char cdn_dag[MAX_BUF_SIZE] = {0};
        double throughput = 0.0;
        double rtt = 0.0;
        parse_updates(buf, cdn_dag, &throughput, &rtt);

        say("#CDN %s:\nthroughput: %lf, rtt: %lf#\n",
            cdn_dag, throughput, rtt);

        insert_new_record(conn, client_dag, cdn_dag, throughput, rtt);

        char cdn_to_pick[MAX_BUF_SIZE] = {0};

        pick_cdn(conn, cdn_to_pick, req_id);

        memset(buf, 0, MAX_BUF_SIZE);
        say("cdn choosed by broker: %s$$\n", cdn_to_pick);
        size_t curr_len = snprintf(buf, MAX_BUF_SIZE - 1, "%s@", cdn_to_pick);
        // send the chosen CDN back to client
        int sent = Xsend(csock, buf, curr_len, 0);
        if (sent < 0) {
            die(-1, "Send error %d on socket %d when returning scheduled CDN\n",
                errno, csock);
        }
    }
}

int get_updates(char *buf, int csock) {
    size_t received = 0;
    size_t offset = 0;
    size_t newline_count = 0;
    int conn_alive = 1;
    int count = 0;
    while (1) {
        count = Xrecv(csock, buf + received, MAX_BUF_SIZE - 1 - received, 0);

        if (count == 0) {
            say("client closed connection\n");
            conn_alive = 0;
            break;
        } else if (count < 0) {
            say("connection with client went wrong\n");
            conn_alive = 0;
            break;
        }

        received += count;
        buf[received] = 0;

        for (size_t i = offset; i < received; i++) {
            if (buf[i] == LINE_END_MARK) newline_count++;
        }
        offset = received;
        if (newline_count == PROPERTIES_COUNT_TO_UPDATE) break;
    }
    if (count < 0) {
        die(-1, "Receive error %d on socket %d\n", errno, csock);
    }
    return conn_alive;
}

void parse_updates(char *buf, char *cdn_dag, double *throughput, double *rtt) {
    int start = 0;
    int end = 0;
    end = find_first(buf, start, LINE_END_MARK);
    memcpy(cdn_dag, buf, end);

    start = end + 1;
    end = find_first(buf, start, LINE_END_MARK);
    buf[end] = 0;
    *throughput = std::atof(&buf[start]);

    start = end + 1;
    end = find_first(buf, start, LINE_END_MARK);
    buf[end] = 0;
    *rtt = std::atof(&buf[start]);
}

int find_first(char *str, size_t offset, char ch) {
    size_t i = offset;
    while (str[i] != 0) {
        if (str[i] == ch) return (int)i;
        i++;
    }
    return -1;
}

/**
 * db utilities
 */

PGconn *connect_db() {
    PGconn *conn = PQconnectdb(DB_CONN_INFO);
    if (PQstatus(conn) == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s\n",
                PQerrorMessage(conn));
        PQfinish(conn);
        exit(1);
    }
    return conn;
}

void close_db(PGconn *conn) {
    PQfinish(conn);
}

void set_up_db() {
    PGconn *conn = connect_db();
    PGresult *res = NULL;

    // PQexec is used to execute SQL query with database that is
    // connected with conn
    // PQresultStatus is used to check the execution result of last SQL query
    // if valid
    // Each time when we called PQexec, and get the result from database, we
    // need to clear the PGresult, otherwise it will lead to memory leak
    res = PQexec(conn, "SET client_min_messages TO WARNING;");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);

    // create a table that record client name and auto-generated request id
    // req_id is auto-generated and is used to identify all the tuples that
    // belong to current connection
    res =
        PQexec(conn, " " \
               "CREATE TABLE IF NOT EXISTS client_of_request( " \
               "    req_id SERIAL            UNIQUE NOT NULL PRIMARY KEY, " \
               "    client_dag VARCHAR(960)  NOT NULL " \
               ");");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);

    // create a table that records all the CDN options that can be used to fetch
    // chunks for the current request
    // use req_id to connect client and CDN options
    res =
        PQexec(conn, " " \
               "CREATE TABLE IF NOT EXISTS cdns_of_request( " \
               "    id SERIAL             UNIQUE NOT NULL PRIMARY KEY, " \
               "    req_id INT            NOT NULL references client_of_request(req_id), " \
               "    cdn_dag VARCHAR(960)  NOT NULL" \
               ");");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);

    // create a table that records the throughput and RTT, from between client
    // and CDN in time order
    res =
        PQexec(conn, " " \
               "CREATE TABLE IF NOT EXISTS client_and_cdn( " \
               "    id SERIAL                     UNIQUE NOT NULL PRIMARY KEY, " \
               "    client_dag VARCHAR(960)       NOT NULL, " \
               "    cdn_dag VARCHAR(960)          NOT NULL, " \
               "    throughput DOUBLE PRECISION   NOT NULL, " \
               "    rtt DOUBLE PRECISION          NOT NULL, " \
               "    report_time TIMESTAMP         DEFAULT CURRENT_TIMESTAMP " \
               ");");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);

    // clear all the data in the old table, and restart the auto-generated id counter
    res =
        PQexec(conn, "TRUNCATE client_of_request, cdns_of_request, client_and_cdn RESTART IDENTITY;");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);
}

size_t add_new_req(PGconn *conn, char *client_dag, std::vector<std::string> &cdn_list) {
    say("cdn_list.size: %zu\n", cdn_list.size());
    PGresult *res = NULL;
    char query[MAX_QUERY_SIZE] = {0};

    snprintf(query, MAX_QUERY_SIZE,
             "INSERT INTO client_of_request (client_dag) VALUES('%s') RETURNING req_id;",
             client_dag);
    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        say("No request id returned\n");
        do_exit(conn, res, 0);
    }
    // get the auto-generated request id, use it to find all the relevant
    // information about current request
    std::string req_id_str(PQgetvalue(res, 0, 0));
    PQclear(res);
    size_t req_id = 0;
    {
        std::stringstream sstream(req_id_str);
        sstream >> req_id;
    }
    say("req_id: %ld\n", req_id);

    // insert all the CDN options to database
    for (size_t i = 0; i < cdn_list.size(); i++) {
        say("i = %zu\n", i);
        snprintf(query, MAX_QUERY_SIZE,
                 "INSERT INTO cdns_of_request (req_id, cdn_dag) VALUES(%zu, '%s');",
                 req_id, cdn_list[i].c_str());
        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            do_exit(conn, res);
        }
        PQclear(res);
    }
    return req_id;
}

void insert_new_record(PGconn *conn, char *client_dag, char *cdn_dag,
                       double throughput, double rtt) {
    PGresult *res = NULL;
    char query[MAX_QUERY_SIZE] = {0};

    snprintf(query, MAX_QUERY_SIZE,
             "INSERT INTO client_and_cdn (client_dag, cdn_dag, throughput, rtt) VALUES('%s', '%s', %f, %f);",
             client_dag, cdn_dag, throughput, rtt);
    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);
}

void pick_cdn(PGconn *conn, char *cdn_to_pick, size_t req_id) {
    PGresult *res = NULL;
    char query[MAX_QUERY_SIZE] = {0};
    // first pick CDNS that have not been used to fetch chunks,
    // then use epsilon-greedy algorithm
    snprintf(query, MAX_QUERY_SIZE, " " \
             "SELECT * " \
             "FROM " \
             "( " \
             "    SELECT client_cdn.cdn_dag " \
             "    FROM " \
             "    ( " \
             "        SELECT client_of_request.client_dag, cdns_of_request.cdn_dag " \
             "        FROM client_of_request " \
             "        INNER JOIN cdns_of_request " \
             "        ON client_of_request.req_id = cdns_of_request.req_id " \
             "        WHERE client_of_request.req_id = %zu " \
             "    ) AS client_cdn " \
             "    WHERE NOT EXISTS ( " \
             "        SELECT 1 " \
             "        FROM client_and_cdn " \
             "        WHERE " \
             "        client_cdn.client_dag = client_and_cdn.client_dag " \
             "        AND " \
             "        client_cdn.cdn_dag = client_and_cdn.cdn_dag " \
             "    ) " \
             ") AS cdn_list " \
             "ORDER BY " \
             "random() " \
             "limit 1;",
             req_id);
    res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        do_exit(conn, res);
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        // pick the best according to historical data
        epsilon_greedy(conn, req_id, cdn_to_pick);
    } else {
        // pick a new one
        say("null cdn: %s\n", PQgetvalue(res, 0, 0));
        std::strncpy(cdn_to_pick, PQgetvalue(res, 0, 0),  MAX_BUF_SIZE);
        PQclear(res);
    }
}

void epsilon_greedy(PGconn *conn, size_t req_id, char *cdn_to_pick) {
    PGresult *res = NULL;
    char query[MAX_QUERY_SIZE] = {0};
    srand (time(NULL));
    double r = ((double) rand() / (RAND_MAX));
    if (r >= EPSILON) {
        // pick the cdn with highest throughput
        snprintf(query, MAX_QUERY_SIZE, " " \
                 "SELECT cdn_dag " \
                 "FROM " \
                 "( " \
                 "    SELECT DISTINCT ON (cdn_dag) cdn_dag, throughput, rtt " \
                 "    FROM " \
                 "    ( " \
                 "        SELECT client_and_cdn.cdn_dag, client_and_cdn.throughput, client_and_cdn.rtt, client_and_cdn.report_time " \
                 "        FROM client_and_cdn " \
                 "        INNER JOIN " \
                 "        ( " \
                 "            SELECT client_of_request.client_dag, cdns_of_request.cdn_dag " \
                 "            FROM client_of_request " \
                 "            INNER JOIN cdns_of_request " \
                 "            ON client_of_request.req_id = cdns_of_request.req_id " \
                 "            WHERE client_of_request.req_id = %zu " \
                 "        ) AS client_cdn " \
                 "        ON client_and_cdn.client_dag = client_cdn.client_dag AND client_and_cdn.cdn_dag = client_cdn.cdn_dag " \
                 "    ) AS report " \
                 "    ORDER BY cdn_dag, report_time DESC " \
                 ") AS latest_report " \
                 "ORDER BY throughput DESC " \
                 "LIMIT 1;",
                 req_id);
        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            do_exit(conn, res);
        }
        say("epsilon_greedy the best: %s\n", PQgetvalue(res, 0, 0));
        strncpy(cdn_to_pick, PQgetvalue(res, 0, 0), MAX_BUF_SIZE);
    } else {
        // pick one randomly
        snprintf(query, MAX_QUERY_SIZE, " " \
                 "SELECT cdns_of_request.cdn_dag " \
                 "FROM client_of_request " \
                 "INNER JOIN cdns_of_request " \
                 "ON client_of_request.req_id = cdns_of_request.req_id " \
                 "WHERE client_of_request.req_id = %zu " \
                 "ORDER BY " \
                 "random() " \
                 "limit 1;",
                 req_id);
        res = PQexec(conn, query);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            do_exit(conn, res);
        }
        say("epsilon_greedy pick random: %s\n", PQgetvalue(res, 0, 0));
        strncpy(cdn_to_pick, PQgetvalue(res, 0, 0), MAX_BUF_SIZE);
    }

    PQclear(res);
}

void release_resource(PGconn *conn, size_t req_id) {
    // delete all the records that have current request id
    PGresult *res = NULL;
    char query[MAX_QUERY_SIZE] = {0};

    snprintf(query, MAX_QUERY_SIZE,
             "DELETE FROM cdns_of_request WHERE req_id = %zu;", req_id);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);

    snprintf(query, MAX_QUERY_SIZE,
             "DELETE FROM client_of_request WHERE req_id = %zu;", req_id);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        do_exit(conn, res);
    }
    PQclear(res);
    PQfinish(conn);
}

void do_exit(PGconn *conn, PGresult *res, int is_error) {
    if (is_error) fprintf(stderr, "%s\n", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    exit(1);
}

