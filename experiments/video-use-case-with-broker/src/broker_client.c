#include "broker_client.h"

void lookup_for_broker(char *broker_name,
                       struct addrinfo **ai_broker_ptr_location,
                       sockaddr_x **sa_broker_ptr_location) {
    char buf[XIA_MAX_DAG_STR_SIZE];
    say("look for broker %s\n", broker_name);
    // get broker's addrinfo from nameserver using broker_name
    if (Xgetaddrinfo(broker_name, NULL, NULL, ai_broker_ptr_location) != 0)
        die(-1, "unable to lookup name %s\n", broker_name);
    *sa_broker_ptr_location = (sockaddr_x*)(*ai_broker_ptr_location)->ai_addr;
    xia_ntop(AF_XIA, *sa_broker_ptr_location, buf, sizeof(buf));
    say("broker find, address: %s\n", buf);
}

int set_conn_to_broker(const sockaddr_x *sa_broker) {
    int broker_sock;
    if ((broker_sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
        die(-1, "unable to create the broker socket\n");
    }

    if (Xconnect(broker_sock, (const struct sockaddr *)sa_broker, sizeof(sockaddr_x)) < 0)
        die(-1, "unable to connect to the broker dag\n");

    say("Xsock %4d connected\n", broker_sock);

    return broker_sock;
}

void init_conn(std::vector<std::string> cdn_lists,
               const char *client_name, const int broker_sock) {
    send_req_info(cdn_lists, client_name, broker_sock);
    wait_for_confirm(broker_sock);
    say("init_conn finished\n");
}

std::string get_init_cdn(std::vector<std::string> cdn_lists) {
    std::srand(std::time(0));
    int idx_rand = std::rand() % cdn_lists.size();
    return cdn_lists[idx_rand];
}

void get_cdn(std::string cdn, char *cdn_to_pick, std::mutex *cdn_mutex_ptr) {
    cdn_mutex_ptr->lock();
    strncpy(cdn_to_pick, cdn.c_str(), MAX_BUF_SIZE - 1);
    cdn_mutex_ptr->unlock();
}

void report(const double throughput, const double rtt, const char *cdn_choosed,
            std::string *cdn, const int broker_sock, std::mutex *mutex_ptr) {
    char buf[MAX_BUF_SIZE] = {0};
    size_t curr_len = 0;
    // @FIXME use string to pass througput and rtt,
    // maybe it is more efficient to use binary format
    curr_len = snprintf(buf, MAX_BUF_SIZE - 1, "%s@%f@%f@",
                        cdn_choosed, throughput, rtt);

    int sent = Xsend(broker_sock, buf, curr_len, 0);
    if (sent < 0 || (size_t)sent != strlen(buf)) {
        die(-1, "Send error %d on socket %d when reporting\n", errno, broker_sock);
    }

    // wait for broker's reply, when broker sends result back, Xpoll will return
    int rc;
    struct pollfd pfds[2];
    pfds[0].fd = broker_sock;
    pfds[0].events = POLLIN;
    if ((rc = Xpoll(pfds, 1, -1)) <= 0) {
        die(-5, "Poll returned %d\n", rc);
    }

    int received = 0;   // bytes that has been received since broker replies
    int count = 0;      // bytes that is returned by last call of Xrecv
    memset(buf, 0, sizeof(buf));

    while ((count = Xrecv(broker_sock, &buf[received], sizeof(buf) - received, 0)) > 0) {
        received += count;
        buf[received] = 0;
        if (buf[received - 1] == LINE_END_MARK) break;   // use @ to indicate the end of reply
    }
    assert(buf[received - 1] == LINE_END_MARK);
    buf[received - 1] = 0;
    // buf now contains the name of cdn that will be used to fetch next chunk

    if (count < 0) {
        die(-1, "Recv error %d on socket %d when reporting\n",
            errno, broker_sock);
    }

    std::string cdn_to_pick_str(buf);
    // in order to update cdn, we need first lock the mutex, because report
    // function will be called in another thread that is separated from the
    // main funtion to fecth video chunks, the main function will also read the
    // value in *cdn to choose cdn
    mutex_ptr->lock();
    *cdn = cdn_to_pick_str;
    mutex_ptr->unlock();
}

void send_req_info(std::vector<std::string> cdn_lists,
                   const char *client_name, const int broker_sock) {
    char buf[MAX_BUF_SIZE] = {0};
    size_t curr_len = 0;

    /**
     * the content in buf(no \n):
     * [client name]@
     * [number of cdn]@
     * [cdn1]@
     * [cdn2]@
     * ...
     * END_MARK
     */
    curr_len = snprintf(buf, MAX_BUF_SIZE - 1, "%s@", client_name);
    curr_len += snprintf(buf + curr_len, MAX_BUF_SIZE - 1 - curr_len, "%zu@", cdn_lists.size());
    for (size_t i = 0; i < cdn_lists.size(); i++) {
        curr_len += snprintf(buf + curr_len, MAX_BUF_SIZE - 1 - curr_len, "%s@", cdn_lists[i].c_str());
    }
    snprintf(buf + curr_len, MAX_BUF_SIZE - 1 - curr_len, END_MARK);

    int sent = Xsend(broker_sock, buf, strlen(buf), 0);
    if (sent < 0) {
        die(-1, "Send error %d on socket %d\n", errno, broker_sock);
    }
}

void wait_for_confirm(const int broker_sock) {
    // wait for broker's reply, when broker sends result back, Xpoll will return
    {
        int rc;
        struct pollfd pfds[2];
        pfds[0].fd = broker_sock;
        pfds[0].events = POLLIN;
        if ((rc = Xpoll(pfds, 1, -1)) <= 0) {
            die(-5, "Poll returned %d\n", rc);
        }
    }

    // expects broker send an END_MARK back to confirm the connection
    char buf[MAX_BUF_SIZE] = {0};
    int count = 0;
    int received = 0;
    while ((count = Xrecv(broker_sock, &buf[received], sizeof(buf) - received, 0)) > 0) {
        received += count;
        buf[received] = 0;
        if (received == END_LEN) break;
    }
    if (count < 0) {
        die(-1, "Send error %d on socket %d\n", errno, broker_sock);
    }
    assert(strncmp(buf, END_MARK, END_LEN) == 0);
}