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

	char buf[XIA_MAXBUF];
	int n;

	/* Wait for CLIENT HELLO */
    if ((n = Xrecv(xssl->sockfd, buf, sizeof(buf), 0)) < 0) {
		ERROR("ERROR receiving CLIENT HELLO");
		return 0;
    }
	if (strcmp("CLIENT HELLO", buf) != 0) {  // TODO: test
		ERROR("ERROR: received message was not CLIENT HELLO");
		return 0;
	}

	/* Send SERVER HELLO */
	sprintf(buf, "SERVER HELLO");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != (int)strlen(buf)) {
		ERROR("ERROR sending SERVER HELLO");
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
	if ((n = Xsend(xssl->sockfd, buf, offset, 0)) != (int)offset) {
		ERROR("ERROR sending public keys");
		return 0;
	}

	DBGF("Sent keys (%u bytes)\n\tkeylen: %u\n\ttempkeylen: %u\n\tsiglen:%u", (unsigned)offset, (unsigned)keybufsize, (unsigned)tempkeybufsize, (unsigned)siglen);


	/* Send SERVER DONE */
	sprintf(buf, "SERVER DONE");
	if ((n = Xsend(xssl->sockfd, buf, strlen(buf), 0)) != (int)strlen(buf)) {
		ERROR("ERROR sending SERVER DONE");
		return 0;
	}


	/* Receive and decrypt pre-master secret */
	size_t expecting = RSA_size(xssl->session_keypair); // TODO: have client send key size?
	n = 0;
	offset = 0;
	while (offset < expecting) {
    	if ((n = Xrecv(xssl->sockfd, &buf[offset], expecting-offset, 0)) < 0) {
			ERROR("ERROR receiving session key");
			return 0;
    	}
		offset += n;
	}

	unsigned char* pms = (unsigned char*)malloc(RSA_size(xssl->session_keypair));
	int plaintext_len;
	if ( (plaintext_len = priv_decrypt(xssl->session_keypair, 
							pms, RSA_size(xssl->session_keypair), 
							(unsigned char*)buf, offset)) == -1) {
		ERROR("ERROR decrypting session key");
		return 0;
	}
	if (plaintext_len != PRE_MASTER_SECRET_LENGTH) {
		ERROR("Decrypted key material is not of size PRE_MASTER_SECRET_LENGTH");
		return 0;
	}
	if (VERBOSITY >= V_DEBUG) {
		DBG("Pre_Master Secret:");
		print_bytes(pms, PRE_MASTER_SECRET_LENGTH);
	}


	/* Init symmetric session ciphers with pre-master secret.
	   Client encrypt context initialized with same key data as
	   server decrypt context and vice versa. */
	uint32_t salt[] = SALT;
	if (aes_init(&pms[PRE_MASTER_SECRET_LENGTH/2], PRE_MASTER_SECRET_LENGTH/2, 
				 pms, PRE_MASTER_SECRET_LENGTH/2, 
				 (unsigned char *)&salt, xssl->en, xssl->de)) {
		ERROR("ERROR initializing AES ciphers");
		return 0;
	}
	free(pms);


	/* For now, omitting SERVER DONE */


	return 1;
}
