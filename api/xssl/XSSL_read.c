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
** @file XSSL_read.c
** @brief implements XSSL_read()
*/

#include "xssl.h"
#include "Xsocket.h"


/**
* @brief Read data from an XSSL connection.
*
* @param xssl
* @param buf Buffer for received data.
* @param num Max number of bytes to read.
*
* @return Number of bytes read on success.
* @return 0 or <0 on failure.
*/
int XSSL_read(XSSL *xssl, void *buf, int num) {

	/* receive up to num bytes of ciphertext.
	   to decrease the chance we don't receive the full message,
	   don't stop unless we're on an AES_BLOCK boundary.
	   FIXME: still possible we don't get all blocks; to fix this we
	   really need an SSL header w/ message size up front */
	unsigned char* ciphertext = (unsigned char *)malloc(num);
	int n = 0;
	int offset = 0;
	while ( offset == 0 || offset % AES_BLOCK_SIZE != 0 ) {
		if ( (n = Xrecv(xssl->sockfd, &ciphertext[offset], num-offset, 0)) <= 0) {
			return n;
		}
		offset += n;
	}


	/* decrpyt the ciphertext */
	int plaintext_len = aes_decrypt(xssl->de,
							(unsigned char*)buf, num,
							ciphertext, n);	

	free(ciphertext);
	return plaintext_len;
}
