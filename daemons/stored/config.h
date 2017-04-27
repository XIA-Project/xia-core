#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>

#define HOST                "127.0.0.1"
#define PORT                8000
#define LISTEN_BACKLOG      1024
#define MAX_CONNECTION      3
#define BUFFER_SIZE         256
#define HEADER_SIZE         4

#define OP_GET              0
#define OP_POST             1

#define MONGODB_URL         "mongodb://localhost:27017?maxpoolsize=10&minpoolsize=1" 
#define MONGODB_DB          "cache"
#define MONGODB_COLLECTION  "cache"

#define SHA1_LENGTH         40  // SHA1: 160 bits = 40 hex char

#define ENABLE_GARBAGE_COLLECTION       true
#define GARBAGE_COLLECTION_TIME_PERIOD  3   // second
#define GARBAGE_COLLECTION_THRESHOLD    10
#define GARBAGE_COLLECTION_PERCENTAGE   50

#endif
