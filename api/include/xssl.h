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
#include <pthread.h>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/evp.h>


/* LOGGING */

#define V_ERROR 0
#define V_WARNING 1
#define V_INFO 2
#define V_DEBUG 3

#ifdef DEBUG
#define VERBOSITY V_DEBUG
#else
#define VERBOSITY V_DEBUG
#endif

#define LOG(levelstr, color, s) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t%s\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), s)
#define LOGF(levelstr, color, fmt, ...) fprintf(stderr, "\033[0;3%dm[ %s ]\033[0m\t[%s:%d (thread %p)]\t" fmt"\n", color, levelstr, __FILE__, __LINE__, (void*)pthread_self(), __VA_ARGS__) 

#if VERBOSITY >= V_INFO
#define INFO(s) LOG("INFO", 2, s)
#define INFOF(fmt, ...) LOGF("INFO", 2, fmt, __VA_ARGS__)
#else
#define INFO(s)
#define INFOF(fmt, ...)
#endif

#if VERBOSITY >= V_DEBUG
#define DBG(s) LOG("DEBUG", 4, s)
#define DBGF(fmt, ...) LOGF("DEBUG", 4, fmt, __VA_ARGS__)
#else
#define DBG(s)
#define DBGF(fmt, ...)
#endif

#if VERBOSITY >= V_ERROR
#define ERROR(s) LOG("ERROR", 1, s)
#define ERRORF(fmt, ...) LOGF("ERROR", 1, fmt, __VA_ARGS__)
#else
#define ERROR(s)
#define ERRORF(fmt, ...)
#endif

#if VERBOSITY >= V_WARNING
#define WARN(s) LOG("WARNING", 3, s)
#define WARNF(fmt, ...) LOGF("WARNING", 3, fmt, __VA_ARGS__)
#else
#define WARN(s)
#define WARNF(fmt, ...)
#endif




#define RSA_KEY_LENGTH 1024  /* in bits */
#define RSA_PUB_EXPONENT 3
#define PRE_MASTER_SECRET_LENGTH 32  /* in bytes */
#define SALT {7509, 9022}


typedef struct {
	RSA *keypair;
	
} XSSL_CTX;


typedef struct {
	XSSL_CTX *ctx;
	RSA *session_keypair;
	int sockfd;
	EVP_CIPHER_CTX *en;  // encryption context
	EVP_CIPHER_CTX *de;  // decryption context
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
int XSSL_shutdown(XSSL *xssl);

int XSSL_encrypt(XSSL *xssl, const void *plaintext, int len, void *ciphertext, int size);
int XSSL_decrypt(XSSL *xssl, const void *ciphertext, int len, void *plaintext, int size);
int XSSL_connect_ip(XSSL *xssl);
int XSSL_accept_ip(XSSL *xssl);


/* Utility Functions (in XSSL_util.c) */
char* SID_from_keypair(RSA *keypair);
void print_keypair(RSA *keypair);
void print_bytes(unsigned char *bytes, size_t len);
uint32_t serialize_rsa_pub_key(RSA *keypair, char *buf, size_t len);
RSA *deserialize_rsa_pub_key(char *buf, size_t len);
unsigned char *hash(char *msg, size_t msg_len);
uint32_t sign(RSA *keypair, char *msg, size_t msg_len, char *sigbuf, int sigbuflen);
int verify(RSA *keypair, char *msg, size_t msg_len, char *sig, size_t sig_len);
int pub_encrypt(RSA *keypair, unsigned char *msg, size_t msg_len, char *ciphertext_buf, int ciphertext_buf_len);
int priv_decrypt(RSA *keypair, unsigned char *msg_buf, int msg_buf_len, unsigned char *ciphertext, size_t ciphertext_len);
int aes_init(unsigned char *en_key_data, int en_key_data_len, unsigned char *de_key_data, int de_key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx);
int aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, int plaintext_len, unsigned char *ciphertext_buf, int ciphertext_buf_len);
int aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext_buf, int plaintext_buf_len, unsigned char *ciphertext, int ciphertext_len);

#endif /* XSSL_H */
