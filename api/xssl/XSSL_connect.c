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
  @file XSSL_connect.c
  @brief Implements XSSL_connect()
*/

#include "xssl.h"
#include "Xsocket.h"
#include "dagaddr.hpp"

/**
* @brief Initiate an XSSL connection with server.
*
* @param xssl
*
* @return 1 on success, <1 on failure
*/
int XSSL_connect(XSSL *xssl) {
printf("<<< XSSL_connect\n");

	char buf[XIA_MAXBUF];
	int n;

	/* Send CLIENT HELLO */
	sprintf(buf, "CLIENT HELLO");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != strlen(buf)) {
		printf("ERROR sending CLIENT HELLO\n");
		return 0;
	}
printf("    sent CLEINT HELLO\n");

	/* Wait for SERVER HELLO */
	// Receive public key + temporary public key signed by server's long-term private key
	// Continue receiving until we see "SERVER DONE"
	int offset = 0;
	while ( offset < 11 || strcmp("SERVER DONE", &buf[offset-11]) != 0 ) {
		n = Xrecv(xssl->sockfd, &buf[offset], sizeof(buf)-offset, 0);
printf("    received %d bytes\n", n);
		if (n < 0) {
			printf("ERROR receiving SERVER HELLO\n");
			return n;
		}
		if (n == 0) {
			printf("ERROR: server closed connection during SERVER HELLO\n");
			return n;
		}

		offset += n;
	}
printf("    Got SERVER HELLO!\n");

	/* Parse public keys from SERVER HELLO */
	offset = strlen("SERVER HELLO");
	char *keys = &buf[offset];  // start of signed portion of message

	uint32_t *keybufsizeptr = (uint32_t*)&buf[offset];
	uint32_t keybufsize = *keybufsizeptr; // TODO: error checking here
	offset += sizeof(uint32_t);
	RSA *pubkey = deserialize_rsa_pub_key(&buf[offset], keybufsize);
	offset += keybufsize;

	uint32_t *tempkeybufsizeptr = (uint32_t*)&buf[offset];
	uint32_t tempkeybufsize = *tempkeybufsizeptr;
	offset += sizeof(uint32_t);
	RSA *sessionPubkey = deserialize_rsa_pub_key(&buf[offset], tempkeybufsize);
	offset += tempkeybufsize;

	uint32_t *siglenptr = (uint32_t*)&buf[offset];
	uint32_t siglen = *siglenptr;
	offset += sizeof(uint32_t);
	char* sig = &buf[offset];
	offset += siglen;
printf("    Received keys:\n\tkeylen: %d\n\ttempkeylen: %d\n\tsiglen: %d\n", keybufsize, tempkeybufsize, siglen);

	/* Verify two things:
	 *	1) hash(key) == SID so we trust the signature
	 *	2) verify the sig so we trust the temp key
	 */
	sockaddr_x *sa = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	socklen_t len = sizeof(sockaddr_x);
	if (Xgetpeername(xssl->sockfd, (struct sockaddr*)sa, &len) < 0) {
		printf("Error in Xgetpeername on socket %d: %s\n", xssl->sockfd, strerror(errno));
		return 0;
	}	
	Graph g(sa);
	Node sid_node = g.get_final_intent();
printf("%s\n", sid_node.to_string().c_str());
	char *sid_from_key_hash = SID_from_keypair(pubkey);
printf("%s\n", sid_from_key_hash);
	if (strcmp(sid_node.to_string().c_str(), sid_from_key_hash) != 0) {
		printf("WARNING! Hash of received public key does not match SID! Closing connection.\n");
		return 0;
	}
	if (verify(pubkey, keys, keybufsize+tempkeybufsize+2*sizeof(uint32_t), sig, siglen) != 1) {
		printf("WARNING! Unable to verify signature on temporary RSA keypair! Closing connection.\n");
		return 0;
	}


	/* Generate AES key and send to server, encrypted with sessionPubKey */
	xssl->aes_key = (unsigned char*)malloc(AES_KEY_LENGTH);
    if (RAND_bytes(xssl->aes_key, AES_KEY_LENGTH) != 1) {
        fprintf(stderr, "ERROR: Couldn't generate AES key\n");
		return 0;
    }
print_aes_key(xssl->aes_key);

	int ciphertext_len;
	if ( (ciphertext_len = encrypt(sessionPubkey, 
							xssl->aes_key, AES_KEY_LENGTH, 
							buf, XIA_MAXBUF)) == -1 ) {
		fprintf(stderr, "ERROR: Unable to encrypt session key\n");
		return 0;
	}
    
	n = 0;
	offset = 0;
	while (offset < ciphertext_len) {
		if ((n = Xsend(xssl->sockfd, &buf[offset], ciphertext_len-offset, 0)) < 0) {
			fprintf(stderr, "ERROR sending session key\n");
			return 0;
		}
		offset += n;
	}

	
	/* For now, omitting CLIENT DONE */


	return 1;
}
