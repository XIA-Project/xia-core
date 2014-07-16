// -*- c-basic-offset: 4; related-file-name: "../../lib/xiasecurity.cc" -*-
#ifndef CLICK_XIASECURITY_HH
#define CLICK_XIASECURITY_HH
#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/vector.hh>
#include <click/xid.hh>


#define MAX_KEYDIR_PATH_LEN 1024
#define XIA_KEYDIR "key"
#define XIA_SHA_DIGEST_STR_LEN SHA_DIGEST_LENGTH*2+1
CLICK_DECLS

/*
// Generate HMAC-SHA1
int xs_HMAC(void* key, int keylen, unsigned char* buf, size_t buflen, unsigned char* hmac, unsigned int* hmac_len);

// Compare two HMACs
int xs_compareHMACs(unsigned char *hmac1, unsigned char *hmac2, unsigned int hmac_len);

// Generate a buffer with random data
int xs_makeRandomBuffer(char *buf, int buf_len);
*/

// Generate SHA1 hash of a given buffer
void xs_getSHA1Hash(unsigned char *data, int data_len, uint8_t* digest, int digest_len);

// Convert a SHA1 digest to a hex string
void xs_hexDigest(uint8_t* digest, int digest_len, char* hex_string, int hex_string_len);

// Verify signature
int xs_isValidSignature(unsigned char *data, size_t datalen, unsigned char *signature, unsigned int siglen, char *xid);
int xs_isValidSignature(unsigned char *data, size_t datalen, unsigned char *signature, unsigned int siglen, uint8_t *pem_pub, int pem_pub_len);

// Sign a given buffer
int xs_sign(char *xid, unsigned char *data, int datalen, unsigned char *signature, unsigned int siglen);

// Read public key from file
int xs_getPubkey(char *xid, uint8_t* pubkey, int *pubkey_len);

CLICK_ENDDECLS

#endif
