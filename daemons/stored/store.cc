/**
 * store
 * =====
 * A non thread-safe cache interface, provides get and post operations.
 *
 * Reference
 * =========
 *
 * MongoDB C Driver API
 * - http://mongoc.org/libmongoc/1.4.2/index.html#api-reference
 * - https://raw.githubusercontent.com/mongodb/mongo-c-driver/master/tests/test-mongoc-client-pool.c
 * Bson:
 * - http://mongoc.org/libbson/1.4.2/
 * - https://github.com/mongodb/libbson/blob/ca2f3ad7548a25580312814ab54bf3e93a9b6a30/src/bson/bson.h
 *
 */

#include "store.h"
#include "bcon-wrapper.h"

bool isValidChecksum(Chunk chunk);

/* Connection Objects */

static mongoc_uri_t *uri;
static mongoc_client_pool_t *pool;

typedef struct _Connection {
    mongoc_client_t      *client;
    mongoc_database_t    *database;
    mongoc_collection_t  *collection;
} Connection;

/* Prototypes */

// garbage collection helper functions
void db_update_accessed_time(Connection *connection, const char *cid);
void db_scan_and_mark_ttl_expired_chunk(Connection *connection);
void db_scan_and_mark_old_chunk(Connection *connection);
void db_mark_chunk_accessed_time_before(Connection *connection, time_t pivot_accessed_time);
time_t get_pivot_accessed_time(Connection *connection, int64_t count);

int time_t_compar(const void *a, const void *b);
int64_t get_collection_count(Connection *connection);
time_t *get_accessed_times(Connection *connection, int64_t count);
void db_mark_chunk(Connection *connection, const char *cid);
void db_check_and_mark_ttl_expired_chunk(Connection *connection,
        const bson_t *doc);

void db_delete_marked_chunk(Connection *connection);

/* Database */

void db_init() {

    mongoc_init();
    uri = mongoc_uri_new(MONGODB_URL);
    pool = mongoc_client_pool_new(uri);

}

void db_deinit() {

    mongoc_uri_destroy(uri);
    mongoc_client_pool_destroy(pool);
    mongoc_cleanup();

}

Connection *retrive_connection() {

    Connection *connection = (Connection *)malloc_w(sizeof(Connection));

    mongoc_client_t *client = mongoc_client_pool_pop(pool);
    connection->client      = client;
    connection->database    = mongoc_client_get_database(client, MONGODB_DB);
    connection->collection  = mongoc_client_get_collection(client, MONGODB_DB, MONGODB_COLLECTION);

    return connection;

}

void release_connection(Connection *connection) {

    mongoc_collection_destroy(connection->collection);
    mongoc_database_destroy(connection->database);
    mongoc_client_pool_push(pool, connection->client);

    free(connection);

}

/* Operation */

Chunk *db_get(const char *cid) {

    // connect to database
    Connection *connection = retrive_connection();
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    bson_oid_t oid;

    // query
    query = bson_new();
    bson_oid_init_from_string (&oid, cid);
    BSON_APPEND_OID(query, "_id", &oid);

    cursor = mongoc_collection_find(collection,
            MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);

    if(!mongoc_cursor_next(cursor, &doc)) {
        bson_destroy(query);
        mongoc_cursor_destroy(cursor);
        release_connection(connection);
        return NULL;
    }

    // update the accessed time
    db_update_accessed_time(connection, cid);

    // parse bson return back to chunk
    Chunk *chunk = bson2chunk(doc, cid);

    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    release_connection(connection);

    return chunk;

}

void db_update_accessed_time(Connection *connection, const char *cid) {

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    bson_t *query, *update;
    bson_oid_t oid;
    bson_error_t error;

    time_t now = time(NULL);
    bson_oid_init_from_string (&oid, cid);
    query = BCON_NEW("_id", BCON_OID(&oid));
    update = BCON_NEW ("$set", "{",
            "accessed_time", BCON_DATE_TIME(now * 1000),
            "}");

    // update
    if (!mongoc_collection_update(collection,
                MONGOC_UPDATE_NONE, query, update, NULL, &error)) {
        syslog(LOG_ERR, "Error: %s\n", error.message);
    }

    // release
    bson_destroy(query);
    bson_destroy(update);

}

bool db_post(Chunk chunk) {

    if(!isValidChecksum(chunk)) {
        syslog(LOG_INFO, "Invalid chunk with wrong checksum\n");
        return false;
    }

    Connection *connection = retrive_connection();
    mongoc_collection_t *collection = connection->collection;

    bson_t *doc;
    doc = bson_new();
    chunk2bson(doc, chunk);

    bson_error_t error;
    if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error)) {
        bson_destroy (doc);
        release_connection(connection);
        syslog(LOG_INFO, "%s\n", error.message);
        return false;
    }

    bson_destroy (doc);

    release_connection(connection);
    return true;

}

void db_cleanup() {

    Connection *connection = retrive_connection();

    db_scan_and_mark_ttl_expired_chunk(connection);
    db_scan_and_mark_old_chunk(connection);
    db_delete_marked_chunk(connection);

    release_connection(connection);

    fflush(stdout);

}

void db_scan_and_mark_ttl_expired_chunk(Connection *connection) {

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;

    // query
    query = bson_new();
    cursor = mongoc_collection_find(collection,
            MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);

    while(mongoc_cursor_next(cursor, &doc)) {
        // check and mark ttl expired chunk
        db_check_and_mark_ttl_expired_chunk(connection, doc);
    }

    // release
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);

}

void db_check_and_mark_ttl_expired_chunk(Connection *connection,
        const bson_t *doc) {

    char *cid;
    uint32_t ttl;
    time_t created_time, now;

    // get ttl
    ttl = get_ttl_from_doc(doc);
    if(ttl == 0) {
        syslog(LOG_ERR, "Error in accessing ttl, skip\n");
        return;
    }

    // get created_time
    created_time = get_created_time_from_doc(doc);
    if(created_time < 0) {
        syslog(LOG_ERR, "Error in accessing created_time, skip\n");
        return;
    }

    // get cid
    cid = get_cid_from_doc(doc);
    if(cid == NULL) {
        syslog(LOG_ERR, "Error in accessing cid, skip\n");
        return;
    }

    // mark it if it's expired
    now = time(NULL);
    syslog(LOG_INFO, "now = %zu, ttl = %"PRIu32", created_time = %zu\n", now, ttl, created_time);
    if(created_time + ttl <= now) {
        db_mark_chunk(connection, cid);
        syslog(LOG_INFO, "marked (ttl expired): cid = %*s, ttl = %u, created_time = %ld\n",
                SHA1_LENGTH, cid, ttl, created_time);
    }

    free(cid);

}

void db_scan_and_mark_old_chunk(Connection *connection) {

    time_t pivot_accessed_time;
    int64_t count = get_collection_count(connection);

    if(count < 0) {
        syslog(LOG_ERR, "Error in counting documents in collection\n");
        return;
    }

    if(count < (int64_t)GARBAGE_COLLECTION_THRESHOLD) {
        syslog(LOG_INFO, "Chunk count doesn't reach the threshold for clean up");
        return;
    }

    pivot_accessed_time = get_pivot_accessed_time(connection, count);
    syslog(LOG_INFO, "pivot accessed_time = %ld\n", pivot_accessed_time);

    db_mark_chunk_accessed_time_before(connection, pivot_accessed_time);

}

time_t get_pivot_accessed_time(Connection *connection, int64_t count) {

    time_t *accessed_times;
    accessed_times = get_accessed_times(connection, count);
    if(accessed_times == NULL) {
        return -1;
    }

    // sort
    qsort(accessed_times, (size_t)count,
            sizeof(time_t), time_t_compar);

    // print for debug
    int64_t i;
    syslog(LOG_INFO, "%"PRId64" documents counted.\n", count);
    for(i=0; i<count; i++) {
        syslog(LOG_INFO, "time = %ld\n", accessed_times[i]);
    }

    int64_t index = count * GARBAGE_COLLECTION_PERCENTAGE / 100;

    // fix corner cases
    if(index < 0) {
        index = 0;
    } else if(index > count - 1) {
        index = count - 1;
    }

    time_t pivot = accessed_times[index];
    free(accessed_times);

    return pivot;

}

int time_t_compar(const void *a, const void *b) {
    // sort time from new to old
    return (*(time_t *)b - *(time_t *)a);
}

void db_mark_chunk_accessed_time_before(Connection *connection, time_t pivot_accessed_time) {

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    char *cid;

    // query
    // {accessed_time:{$lt:ISODate()}}
    query = BCON_NEW("$query", "{",
                "accessed_time", "{",
                    "$lt", BCON_DATE_TIME(pivot_accessed_time * 1000),
                "}",
            "}");

    cursor = mongoc_collection_find(collection,
            MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);

    while(mongoc_cursor_next(cursor, &doc)) {
        cid = get_cid_from_doc(doc);
        if(cid == NULL) {
            syslog(LOG_ERR, "Error in accessing cid, skip\n");
            return;
        }
        db_mark_chunk(connection, cid);
        syslog(LOG_INFO, "marked: cid = %*s (accessed time too old)\n", SHA1_LENGTH, cid);
    }

    // release
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);

}

int64_t get_collection_count(Connection *connection) {

    bson_t *query;
    bson_error_t error;

    query = bson_new();
    mongoc_collection_t *collection = connection->collection;

    return mongoc_collection_count(collection,
            MONGOC_QUERY_NONE, query, 0, 0, NULL, &error);

}

time_t *get_accessed_times(Connection *connection, int64_t count) {

    int64_t i = 0;
    time_t *accessed_times = (time_t *)malloc_w(sizeof(time_t) * count);

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;

    // query
    query = bson_new();
    cursor = mongoc_collection_find(collection,
            MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);

    while(mongoc_cursor_next(cursor, &doc)) {

        time_t accessed_time = get_accessed_time_from_doc(doc);
        if(accessed_time < 0) {
            syslog(LOG_ERR, "Error in accessing accessed_time\n");
            return NULL;
        }

        accessed_times[i] =accessed_time;
        i++;

    }

    // release
    bson_destroy(query);
    mongoc_cursor_destroy(cursor);

    return accessed_times;
}

void db_mark_chunk(Connection *connection, const char *cid) {

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    bson_t *query, *update;
    bson_oid_t oid;
    bson_error_t error;

    bson_oid_init_from_string (&oid, cid);
    query = BCON_NEW("_id", BCON_OID(&oid));
    update = BCON_NEW ("$set", "{",
            "is_garbage", BCON_BOOL(true),
            "}");

    // update
    if (!mongoc_collection_update(collection,
                MONGOC_UPDATE_NONE, query, update, NULL, &error)) {
        syslog(LOG_ERR, "Error: %s\n", error.message);
    }

    // release
    bson_destroy(query);
    bson_destroy(update);

}

void db_delete_marked_chunk(Connection *connection) {

    int64_t count;

    // get collection
    mongoc_collection_t *collection = connection->collection;

    // variable setup
    bson_t *doc;
    bson_error_t error;

    doc = bson_new();
    BSON_APPEND_BOOL(doc, "is_garbage", true);

    count = mongoc_collection_count(collection,
            MONGOC_QUERY_NONE, doc, 0, 0, NULL, &error);

    if(count < 0) {
        fprintf (stderr, "%s\n", error.message);
        return;
    }

    if(count == 0) {
        // nothing to delete
        return;
    }

    syslog(LOG_INFO, "Garbage collector is deleting %ld chunk items\n", count);

    // update
    if (!mongoc_collection_remove(collection,
                MONGOC_REMOVE_NONE, doc, NULL, &error)) {
        syslog(LOG_ERR, "Error: %s\n", error.message);
    }

    // release
    if(doc) {
        bson_destroy(doc);
    }

}

/* Helper */

bool isValidChecksum(Chunk chunk) {
    // TODO
    return true;
}
