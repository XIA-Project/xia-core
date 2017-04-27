/**
 * stored
 * ======
 * Cache daemon which reads operation requestions from socket
 */
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>

#include "store.h"

#include "config.h"
#include "wrapper.h"
#include "helper.h"

#include "chunk.pb.h"
#include "operation.pb.h"

static volatile bool running = true;
sem_t sem;

/* Garbage collector */
void *garbage_collection_worker(void *) {

    while(true) {

        sleep(GARBAGE_COLLECTION_TIME_PERIOD);
        db_cleanup();

        if(!running) {
            break;
        }

    }

    return NULL;

}

/* Operation Handlers */

void operation_get_handler(int clientfd, const char *cid);
void operation_post_handler(int clientfd, Chunk chunk);

void operation_handler(int clientfd) {

    // get len
    size_t len = read_len(clientfd);
    if(len == 0) {
        syslog(LOG_DEBUG, "Error while reading socket\n");
        return;
    }

    // get content
    uint8_t *content = read_content(clientfd, len);
    if(content == NULL) {
        syslog(LOG_DEBUG, "Error found while reading content\n");
        return;
    }

    // unpack operation
    std::string content_str(content, content + len);

    Operation operation;
    operation.ParseFromString(content_str);

    // run different operation based on the op field
    if(operation.op() == OP_GET) {
        const char *cid = operation.cid().c_str();
        operation_get_handler(clientfd, cid);
    } else if(operation.op() == OP_POST) {
        operation_post_handler(clientfd, operation.chunk());
    } else {
        syslog(LOG_DEBUG, "Error: unseen operation\n");
    }

    // free
    free(content);

}

void operation_get_handler(int clientfd, const char *cid) {

    // get requested chunk
    Chunk *chunk = db_get(cid);

    // not found
    if(chunk == NULL) {
        syslog(LOG_DEBUG, "Requested chunk not found\n");
        return;
    }

    // parse response into proper buffer
    Buffer *buffer = (Buffer *)malloc_w(sizeof(Buffer));
    buffer->len = chunk->ByteSize();
    std::string data_str;
    chunk->SerializeToString(&data_str);
    buffer->data = (uint8_t *)malloc_w(buffer->len);
    memcpy(buffer->data, data_str.c_str(), buffer->len);

    // write to the client socket
    write_socket(clientfd, buffer);
    free_buffer(buffer);

}

void operation_post_handler(int clientfd, Chunk chunk) {
    db_post(chunk);
}

/* Worker Flow */
void *worker(void *clientfd_p) {

    // parse parameters and release resources
    int clientfd = *((int *) clientfd_p);
    pthread_detach_w(pthread_self());   // no pthread_join
    free(clientfd_p);

    // run job
    sem_wait_w(&sem);
    syslog(LOG_DEBUG, "Connected\n");
    operation_handler(clientfd);
    sem_post_w(&sem);

    // close
    close(clientfd);
    return NULL;

}

/* Main Flow */

void signal_handler(int dummy) {
    running = false;
    connect_to(HOST, PORT); // to terminate accept in main loop
}

void init() {

    signal(SIGINT, signal_handler);

    sem_init_w(&sem, 0, MAX_CONNECTION);
    db_init();
}

void deinit() {

    syslog(LOG_DEBUG, "shutting down...");

    sem_destroy(&sem);
    db_deinit();
}

int main(int argc, char **argv) {

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    int listenfd, *clientfd_p;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    pthread_t thread;

    //
    init();

    // garbage collection worker
    if(ENABLE_GARBAGE_COLLECTION) {
        pthread_create_w(&thread, NULL, garbage_collection_worker, NULL);
    }

    // start listening
    listenfd = listen_on_w(PORT);
    syslog(LOG_DEBUG, "Listening on %d...\n", PORT);

    while(true) {

        // wait for connection
        client_addr_len = sizeof(client_addr);
        clientfd_p = (int *) malloc_w(sizeof(int));
        *clientfd_p = accept_w(listenfd,
                (SA *) &client_addr,
                &client_addr_len);

        if(!running) {
            break;
        }

        // create new create per new connection
        pthread_create_w(&thread, NULL, worker, clientfd_p);

    }

    // soft shutdown
    deinit();
    return EXIT_SUCCESS;

}
