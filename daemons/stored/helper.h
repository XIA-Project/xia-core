#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "wrapper.h"

#include "chunk.pb.h"
#include "operation.pb.h"

typedef struct _Buffer {
    size_t len;
    uint8_t *data;
} Buffer;

void print_chunk(Chunk chunk);

size_t read_len(int clientfd);
uint8_t *read_content(int clientfd, size_t len);

void write_socket(int sockfd, Buffer *buffer);
void free_buffer(Buffer *buffer);

#endif
