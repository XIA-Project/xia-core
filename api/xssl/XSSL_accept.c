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
  @file XSSL_accept.c
  @brief Implements XSSL_accept()
*/

#include "xssl.h"
#include "Xsocket.h"

/**
* @brief Accept an incoming XSSL connection.
*
* @param xssl
*
* @return 1 on success, <1 on failure
*/
int XSSL_accept(XSSL *xssl) {
printf("<<< XSSL_accept\n");

	char buf[XIA_MAXBUF];
	int n;

	/* Wait for CLIENT HELLO */
    if ((n = Xrecv(xssl->sockfd, buf, sizeof(buf), 0)) < 0) {
		printf("ERROR receiving CLIENT HELLO\n");
		return 0;
    }
printf("    received %d bytes\n", n);
	if (strcmp("CLIENT HELLO", buf) != 0) {  // TODO: test
		printf("ERROR: received message was not CLIENT HELLO\n");
		return 0;
	}
printf("    it was a CLIENT HELLO!\n");


	/* Send SERVER HELLO */
	sprintf(buf, "SERVER HELLO");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != strlen(buf)) {
		printf("ERROR sending SERVER HELLO\n");
		return 0;
	}


	/* Send public key + temporary public key signed by long-term private key */
	
	//
	// Copy the following into buf:
	// keylen || key || tempkeylen || tempkey || siglen || sig
	//
	size_t offset = 0;

	uint32_t keybufsize = serialize_rsa_pub_key(xssl->ctx->keypair, &buf[sizeof(uint32_t)], XIA_MAXBUF-offset-sizeof(uint32_t));
	uint32_t* keybufsizeptr = (uint32_t*)buf;
	*keybufsizeptr = keybufsize;
	offset += (sizeof(uint32_t) + keybufsize);
	
	uint32_t tempkeybufsize = serialize_rsa_pub_key(xssl->session_keypair, &buf[sizeof(uint32_t) + offset], XIA_MAXBUF-offset-sizeof(uint32_t));
	uint32_t* tempkeybufsizeptr = (uint32_t*)&buf[offset];
	*tempkeybufsizeptr = tempkeybufsize;
	offset += (sizeof(uint32_t) + tempkeybufsize);

	uint32_t siglen = sign(xssl->ctx->keypair, buf, offset, &buf[offset+sizeof(uint32_t)], XIA_MAXBUF-offset-sizeof(uint32_t));
	uint32_t* siglenptr = (uint32_t*)&buf[offset];
	*siglenptr = siglen;
	offset += sizeof(uint32_t);
	offset += siglen;

	// Send it!
	if ((n = Xsend(xssl->sockfd, buf, offset, 0)) != offset) {
		printf("ERROR sending public keys\n");
		return 0;
	}
printf("    sent keys (%d bytes)\n\tkeylen: %d\n\ttempkeylen: %d\n\tsiglen:%d\n", offset, keybufsize, tempkeybufsize, siglen);


	/* Send SERVER DONE */
	sprintf(buf, "SERVER DONE");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != strlen(buf)) {
		printf("ERROR sending SERVER DONE\n");
		return 0;
	}


	/* Receive and decrypt session key */
	size_t expecting = RSA_size(xssl->session_keypair); // TODO: have client send key size?
	n = 0;
	offset = 0;
	while (offset < expecting) {
    	if ((n = Xrecv(xssl->sockfd, &buf[offset], expecting-offset, 0)) < 0) {
			fprintf(stderr, "ERROR receiving session key\n");
			return 0;
    	}
		offset += n;
	}

	unsigned char* plaintext_buf = (unsigned char*)malloc(RSA_size(xssl->session_keypair));
	int plaintext_len;
	if ( (plaintext_len = decrypt(xssl->session_keypair, 
							plaintext_buf, RSA_size(xssl->session_keypair), 
							(unsigned char*)buf, offset)) == -1) {
		fprintf(stderr, "ERROR decrypting session key\n");
		return 0;
	}
	if (plaintext_len != AES_KEY_LENGTH) {
		fprintf(stderr, "ERRLR: decrypted key is not of size AES_KEY_LENGTH\n");
		return 0;
	}
	xssl->aes_key = (unsigned char*)malloc(AES_KEY_LENGTH);
	memcpy(xssl->aes_key, plaintext_buf, AES_KEY_LENGTH);
	free(plaintext_buf);
print_aes_key(xssl->aes_key);


	/* For now, omitting SERVER DONE */


	return 1;
}
