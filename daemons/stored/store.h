#ifndef __CACHE_H__
#define __CACHE_H__

#include <stdbool.h>
#include <bson.h>
#include <mongoc.h>
#include <stdbool.h>
#include <syslog.h>

#include "proto/chunk.pb.h"
#include "proto/operation.pb.h"

#include "config.h"
#include "wrapper.h"
#include "bcon-wrapper.h"

void db_init();
void db_deinit();

Chunk *db_get(const char *cid);
bool db_post(Chunk chunk);

void db_cleanup();

#endif
