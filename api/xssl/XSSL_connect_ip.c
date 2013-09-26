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

#include "xssl.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

/**
* @brief Initiate an XSSL connection with server.
*
* @param xssl
*
* @return 1 on success, <1 on failure
*/
int XSSL_connect_ip(XSSL *xssl) {

	char buf[65000];
	int n;

	/* Send CLIENT HELLO */
	sprintf(buf, "CLIENT HELLO");
	if ((n = send(xssl->sockfd, buf, strlen(buf), 0)) != strlen(buf)) {
		ERROR("ERROR sending CLIENT HELLO");
		return 0;
	}

	/* Wait for SERVER HELLO */
	// Receive public key + temporary public key signed by server's long-term private key
	// Continue receiving until we see "SERVER DONE"
	int offset = 0;
	while ( offset < 11 || strcmp("SERVER DONE", &buf[offset-11]) != 0 ) {
		n = recv(xssl->sockfd, &buf[offset], sizeof(buf)-offset, 0);
		if (n < 0) {
			ERROR("ERROR receiving SERVER HELLO");
			return n;
		}
		if (n == 0) {
			ERROR("ERROR: server closed connection during SERVER HELLO");
			return n;
		}

		offset += n;
	}

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
	DBGF("Received keys:\n\tkeylen: %d\n\ttempkeylen: %d\n\tsiglen: %d", keybufsize, tempkeybufsize, siglen);

	// verify the sig so we trust the temp key
	if (verify(pubkey, keys, keybufsize+tempkeybufsize+2*sizeof(uint32_t), sig, siglen) != 1) {
		WARN("Unable to verify signature on temporary RSA keypair! Closing connection.");
		return 0;
	}


	/* Generate pre-master secret and send to server, encrypted with sessionPubKey */
	unsigned char* pms = (unsigned char*)malloc(PRE_MASTER_SECRET_LENGTH);
    if (RAND_bytes(pms, PRE_MASTER_SECRET_LENGTH) != 1) {
        ERROR("ERROR: Couldn't generate pre-master secret");
		return 0;
    }
	if (VERBOSITY >= V_DEBUG) {
		DBG("Pre-Master Secret:");
		print_bytes(pms, PRE_MASTER_SECRET_LENGTH);
	}

	int ciphertext_len;
	if ( (ciphertext_len = pub_encrypt(sessionPubkey, 
							pms, PRE_MASTER_SECRET_LENGTH, 
							buf, 65000)) == -1 ) {
		ERROR("ERROR: Unable to encrypt session key");
		return 0;
	}
    
	n = 0;
	offset = 0;
	while (offset < ciphertext_len) {
		if ((n = send(xssl->sockfd, &buf[offset], ciphertext_len-offset, 0)) < 0) {
			ERROR("ERROR sending pre-master secret");
			return 0;
		}
		offset += n;
	}
	

	/* Init symmetric session ciphers with pre-master secret.
	   Client encrypt context initialized with same key data as
	   server decrypt context and vice versa. */
	uint32_t salt[] = SALT;
	if (aes_init(pms, PRE_MASTER_SECRET_LENGTH/2, 
				 &pms[PRE_MASTER_SECRET_LENGTH/2], PRE_MASTER_SECRET_LENGTH/2, 
				 (unsigned char *)&salt, xssl->en, xssl->de)) {
		ERROR("ERROR initializing AES ciphers");
		return 0;
	}
	free(pms);

	
	/* For now, omitting CLIENT DONE */


	return 1;
}
