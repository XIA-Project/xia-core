/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
  @file xssl.h
  @brief XSSL API Header File
*/

#ifndef XSSL_H
#define XSSL_H

#include <stdint.h>
#include <errno.h>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

#define RSA_KEY_LENGTH 1024  /* in bits */
#define RSA_PUB_EXPONENT 3
#define AES_KEY_LENGTH 16  /* in bytes */


typedef struct {
	RSA *keypair;
	
} XSSL_CTX;


typedef struct {
	XSSL_CTX *ctx;
	RSA *session_keypair;
	int sockfd;
	unsigned char *aes_key;
} XSSL;


/* SSL Functions */
XSSL_CTX *XSSL_CTX_new();
void XSSL_CTX_free(XSSL_CTX *ctx);

XSSL *XSSL_new(XSSL_CTX *ctx);
void XSSL_free(XSSL *xssl);

int XSSL_set_fd(XSSL *xssl, int fd);
int XSSL_accept(XSSL *xssl);
int XSSL_connect(XSSL *xssl);
int XSSL_write(XSSL *xssl, const void *buf, int num);
int XSSL_read(XSSL *xssl, void *buf, int num);


/* Utility Functions (in XSSL_util.c) */
char* SID_from_keypair(RSA *keypair);
void print_keypair(RSA *keypair);
void print_aes_key(unsigned char *key);
uint32_t serialize_rsa_pub_key(RSA *keypair, char *buf, size_t len);
RSA *deserialize_rsa_pub_key(char *buf, size_t len);
unsigned char *hash(char *msg, size_t msg_len);
uint32_t sign(RSA *keypair, char *msg, size_t msg_len, char *sigbuf, int sigbuflen);
int verify(RSA *keypair, char *msg, size_t msg_len, char *sig, size_t sig_len);
int encrypt(RSA *keypair, unsigned char *msg, size_t msg_len, char *ciphertext_buf, int ciphertext_buf_len);
int decrypt(RSA *keypair, unsigned char *msg_buf, size_t msg_buf_len, unsigned char *ciphertext, size_t ciphertext_len);

#endif /* XSSL_H */
