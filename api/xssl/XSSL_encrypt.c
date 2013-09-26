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
** @file XSSL_encrypt.c
** @brief implements XSSL_encrypt()
*/

#include "xssl.h"
#include "Xsocket.h"


/**
* @brief Encrypt a buffer of data.
*
* Note that this is a non-standard call. It can be used to encrypt a buffer
* of data without sending it as XSSL_write() would.
*
* @param xssl
* @param plaintext  Data to encrypt
* @param len  Size of data to encrypt
* @param ciphertext  Buffer to be filled with ciphertext
* @param size Size of ciphertext buffer
*
* @return Number of ciphertext bytes placed in buf on success
* @return 0 or <0 on failure
*/
int XSSL_encrypt(XSSL *xssl, const void *plaintext, int len, void *ciphertext, int size) {

	/* encrypt data to send */
	int ciphertext_len = aes_encrypt(xssl->en, 
							(unsigned char*)plaintext, len,
							(unsigned char*)ciphertext, size);

	if (ciphertext_len <= 0) {
		ERROR("ERROR encrypting message");
	}

	return ciphertext_len;
}
