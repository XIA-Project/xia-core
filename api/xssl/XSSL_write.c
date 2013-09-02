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
** @file XSSL_write.c
** @brief implements XSSL_write()
*/

#include "xssl.h"
#include "Xsocket.h"


/**
* @brief Write data to an XSSL connection.
*
* @param xssl
* @param buf Data to send.
* @param num Number of bytes to send.
*
* @return Number of bytes (before encryption) written to the connection on success.
* @return 0 or <0 on failure
*/
int XSSL_write(XSSL *xssl, const void *buf, int num) {

	/* encrypt data to send */
	unsigned char *ciphertext = (unsigned char*)malloc(XIA_MAXBUF);
	int ciphertext_len = aes_encrypt(xssl->en, 
							(unsigned char*)buf, num,
							ciphertext, XIA_MAXBUF);

	if (ciphertext_len <= 0) {
		fprintf(stderr, "ERROR encrypting message\n");
		return ciphertext_len;
	}


	/* send the ciphertext.
	   can't send partial block, or other side can't decrypt! */
	int offset = 0;
	int n = 0;
	while (offset < ciphertext_len) {
		if ( (n = Xsend(xssl->sockfd, &ciphertext[offset], ciphertext_len-offset, 0)) < 0) {
			fprintf(stderr, "ERROR sending ciphertext\n");
			return -1;
		}
		offset += n;
	}
	
	free(ciphertext);
	return num; // return num bytes of *plain text* we sent
}
