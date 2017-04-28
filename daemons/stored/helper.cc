/**
 * helper
 * ======
 * Helper functions which can be applied by different components
 */
#include "helper.h"

/* Log & Debug */

void print_chunk(Chunk chunk) {

    unsigned i;

    printf("[Chunk]\n");
    printf("\tcid = %.*s\n", SHA1_LENGTH, chunk.cid().c_str());
    printf("\tsid = %.*s\n", SHA1_LENGTH, chunk.sid().c_str());
    printf("\tcontent = %.*s (%zu)\n", (int)chunk.content().length(), chunk.content().c_str(), chunk.content().length());
    printf("\t");
    for(i=0;i<chunk.content().length();i++) {
        printf("[%02x] ", chunk.content().at(i));
    }
    printf("\n");
    printf("\tttl = %u, initial_seq = %u\n", chunk.ttl(), chunk.initial_seq());

}

void print_buffer(unsigned len, uint8_t *buffer) {

    unsigned i;

    printf("> %d,\t", len);
    for(i=0; i<len; i++) {
        if(i % 20 == 0) {
            printf("\n");
        }
        printf("[%02x]  ", buffer[i]);
    }

    printf("\n");

}

/* Socket */

/*
 * Generate simple packet with header size = 4 (contains body size)
 */
uint8_t *generate_packet(Buffer *buffer) {

    uint8_t *packet = (uint8_t *)malloc_w(HEADER_SIZE + buffer->len);

    // header (len)
    packet[0] = (buffer->len >> 24) & 0xff;
    packet[1] = (buffer->len >> 16) & 0xff;
    packet[2] = (buffer->len >> 8) & 0xff;
    packet[3] = (buffer->len >> 0) & 0xff;

    // content
    memcpy(packet + HEADER_SIZE, buffer->data, buffer->len);

    return packet;

}

/*
 * First 4 bytes of the data indicates the len
 */
size_t read_len(int clientfd) {

    uint8_t buffer[HEADER_SIZE];
    int n_read = recv(clientfd, buffer, HEADER_SIZE, 0);

    if(n_read != HEADER_SIZE) {
        printf("n_read = %d\n", n_read);
        return 0;
    }

    unsigned len = 0, i;
    for(i=0; i<HEADER_SIZE; i++) {
        len += buffer[i];
        if(i != HEADER_SIZE - 1) {
            len <<= 8;
        }
        printf("%u, %02x, len = %x\n", i, buffer[i], len);
    }

    return len;
}

/*
 * Read following content with specific len, if failed, return NULL
 */
uint8_t *read_content(int clientfd, size_t len) {

    int n_read, n_read_total = 0;
    uint8_t buffer[BUFFER_SIZE];
    uint8_t *content = (uint8_t *)malloc(len);
    uint8_t *iterator = content;
    size_t left = len;

    while(true) {

        n_read = recv(clientfd, buffer, BUFFER_SIZE, 0);

        if(n_read == -1) {
            printf("Error in reading content\n");
            return NULL;
        } else if(n_read == 0) {    // EOF
            break;
        }

        memcpy(iterator, buffer, n_read);
        iterator += n_read;
        n_read_total += n_read;
        left -= n_read;

        if(left == 0) {
            break;
        }

    }

    print_buffer(n_read_total, buffer);

    printf("Read %d bytes for content\n", n_read_total);
    return content;

}

/*
 * Write buffer to socket
 */
void write_socket(int sockfd, Buffer *buffer) {

    // packet
    // uint8_t *packet = malloc_w(HEADER_SIZE + buffer->len);
    // packet = generate_packet(buffer);

    uint8_t *packet = generate_packet(buffer);
    // write
    int n = write_w(sockfd, packet, HEADER_SIZE + buffer->len);

    printf("Write %d bytes succeed\n", n);
    free(packet);

}

/*
 * Free buffer
 */
void free_buffer(Buffer *buffer) {
    if(buffer == NULL) {
        return;
    }
    if(buffer->data != NULL) {
        free(buffer->data);
    }
    free(buffer);
}

/*
void free_chunk(Chunk *chunk) {
    if(chunk == NULL) {
        return;
    }
    if(chunk->cid != NULL) {
        free(chunk->cid);
    }
    if(chunk->sid != NULL) {
        free(chunk->sid);
    }
    if(chunk->content.len != 0) {
        free(chunk->content.data);
    }
    free(chunk);
}
*/
