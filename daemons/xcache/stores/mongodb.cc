#include <syslog.h>
#include <sys/time.h>

#include "store.h"
#include "mongodb.h"
#include "helper.h"
#include "wrapper.h"

#include "chunk.pb.h"
#include "operation.pb.h"

void set_chunk(Chunk *chunk, xcache_meta *meta, const std::string *data);

void set_post_operation(Operation *operation, Chunk *chunk);
void set_get_operation(Operation *operation, std::string cid);

int send_operation(Operation *operation);
std::string read_response(int sockfd);

MongodbStore::MongodbStore()
{
    syslog(LOG_INFO, "MongodbStore::init");
}

/*
 * store: encrypt the meta and data into chunk object attached to an operation
 * object, then send this operation object to mongodb interface using domain
 * socket (blocking).
 */
int MongodbStore::store(xcache_meta *meta, const std::string *data)
{

    syslog(LOG_INFO, "MongodbStore::store");

    Chunk *chunk = new Chunk();
    Operation *operation = new Operation();

    set_chunk(chunk, meta, data);
    set_post_operation(operation, chunk);

    int sockfd = send_operation(operation);
    std::string response = read_response(sockfd);

    return response.compare(SUCCEED_RESPONSE) == 0;

}

/*
 * get: encrypt the query cid into an operation object, then retrieve the result
 * from the mongodb interface (blocking).
 */
std::string MongodbStore::get(xcache_meta *meta)
{

    syslog(LOG_INFO, "MongodbStore::get, cid=%s", meta->get_cid().c_str());

    Operation *operation = new Operation();
    set_get_operation(operation, meta->get_cid());
    try {
        int sockfd = send_operation(operation);
        Chunk chunk;
        chunk.ParseFromString(read_response(sockfd));
        return chunk.content();
    } catch(int e) {
        throw e;
    }

}

void set_chunk(Chunk *chunk, xcache_meta *meta, const std::string *data)
{

    // Chunk
    chunk->set_cid(meta->get_cid().c_str());
    chunk->set_content(data->c_str());

    chunk->set_sid(meta->dest_sid());
    chunk->set_initial_seq(meta->seq());
    chunk->set_ttl(meta->ttl());

    // now
    struct timeval now;
    gettimeofday(&now, NULL);
    long int now_ms = now.tv_sec * 1000 + now.tv_usec / 1000;

    chunk->set_created_time(now_ms);
    chunk->set_accessed_time(now_ms);

}

void set_post_operation(Operation *operation, Chunk *chunk)
{

    // Operation
    operation->set_op(OP_POST);
    operation->set_cid(chunk->cid().c_str());
    operation->set_allocated_chunk(chunk);

}

void set_get_operation(Operation *operation, std::string cid)
{

    // Operation
    operation->set_op(OP_GET);
    operation->set_cid(cid);

}

int send_operation(Operation *operation)
{

    Buffer *buffer = (Buffer *)malloc_w(sizeof(Buffer));

    // Serialize
    std::string data_str;
    buffer->len = operation->ByteSize();
    buffer->data = (uint8_t *)malloc_w(buffer->len);
    operation->SerializeToString(&data_str);
    memcpy(buffer->data, data_str.c_str(), buffer->len);

    // socket
    int sockfd = connect_to(HOST, PORT);

    // send request
    write_socket(sockfd, buffer);
    free_buffer(buffer);

    return sockfd;

}

std::string read_response(int sockfd)
{

    // read response
    // get len
    size_t len = read_len(sockfd);
    if(len == 0) {
        // not found
        syslog(LOG_INFO, "Error: empty response\n");
        throw -1;
    }
    syslog(LOG_INFO, "response content len = %zu", len);

    // get content
    uint8_t *content = read_content(sockfd, len);
    close(sockfd);

    if(content == NULL) {
        // not found
        syslog(LOG_INFO, "Error in reading content\n");
        throw -1;
    }

    std::string content_str(content, content + len);
    syslog(LOG_INFO, "string: %s", content_str.c_str());
    return content_str;

}

std::string MongodbStore::get_partial(xcache_meta *meta, off_t off, size_t len)
{
    syslog(LOG_INFO, "MongodbStore::get_partial");
    return "";
}

int MongodbStore::remove(xcache_meta *meta)
{
    syslog(LOG_INFO, "MongodbStore::remove");
    return 0;
}

void MongodbStore::print(void)
{
    syslog(LOG_INFO, "MongodbStore::print");
}

