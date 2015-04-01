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

	char buf[XIA_MAXBUF];
	int n;

	/* Send CLIENT HELLO */
	sprintf(buf, "CLIENT HELLO");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != (int)strlen(buf)) {
		ERROR("ERROR sending CLIENT HELLO");
		return 0;
	}

	/* Wait for SERVER HELLO */
	// Receive public key + temporary public key signed by server's long-term private key
	// Continue receiving until we see "SERVER DONE"
	int offset = 0;
	while ( offset < 11 || strcmp("SERVER DONE", &buf[offset-11]) != 0 ) {
		n = Xrecv(xssl->sockfd, &buf[offset], sizeof(buf)-offset, 0);
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

	/* Verify two things:
	 *	1) hash(key) == SID so we trust the signature
	 *	2) verify the sig so we trust the temp key
	 */
	sockaddr_x *sa = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	socklen_t len = sizeof(sockaddr_x);
	if (Xgetpeername(xssl->sockfd, (struct sockaddr*)sa, &len) < 0) {
		ERRORF("Error in Xgetpeername on socket %d: %s", xssl->sockfd, strerror(errno));
		return 0;
	}	
	Graph g(sa);
	Node sid_node = g.get_final_intent();
	DBGF("SID:          %s", sid_node.to_string().c_str());
	char *sid_from_key_hash = SID_from_keypair(pubkey);
	DBGF("Pub key hash: %s", sid_from_key_hash);
	if (strcmp(sid_node.to_string().c_str(), sid_from_key_hash) != 0) {
		WARN("Hash of received public key does not match SID! Closing connection.");
		return 0;
	}
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
							buf, XIA_MAXBUF)) == -1 ) {
		ERROR("ERROR: Unable to encrypt session key");
		return 0;
	}
    
	n = 0;
	offset = 0;
	while (offset < ciphertext_len) {
		if ((n = Xsend(xssl->sockfd, &buf[offset], ciphertext_len-offset, 0)) < 0) {
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
