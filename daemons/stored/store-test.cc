/**
 * store-test
 * ==========
 * Test code for cace, which does get then post operation calls.
 */

#include <stdio.h>
#include <time.h>
#include "store.h"
#include "helper.h"

#include "proto/chunk.pb.h"
#include "proto/operation.pb.h"

void db_post_test() {

    time_t now;
    time(&now);

    // Create dummy chunk
    Chunk chunk;
    chunk.set_cid("93e9ce27e198605616ef247aa5aeb411dcac065c");
    chunk.set_sid("5b9f884a931a2c8f161c24739393f71895d645c1");
        
    chunk.set_content((const char *)malloc_w(26));

    chunk.set_initial_seq(200);

    chunk.set_ttl(300);
    chunk.set_created_time(now);
    chunk.set_accessed_time(now);

    // Run db post operation
    print_chunk(chunk);
    db_post(chunk);

}

void db_get_test() {

    // Retrive the dummy chunk
    Chunk *chunk = db_get("93e9ce27e198605616ef247aa5aeb411dcac065b");
    if(chunk == NULL) {
        printf("Chunk not found\n");
    } else {
        print_chunk(*chunk);
        delete chunk;
    }

    return;

}

int main() {

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    db_init();

    db_post_test();
    db_get_test();

    db_deinit();

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
