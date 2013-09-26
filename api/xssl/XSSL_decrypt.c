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
** @file XSSL_decrypt.c
** @brief implements XSSL_decrypt()
*/

#include "xssl.h"
#include "Xsocket.h"


/**
* @brief Decrypt a buffer of ciphertext.
*
* Note that this is a non-standard call. It can be used to decrypt a buffer
* of data rather than reading data from the network as XSSL_read() would.
*
* @param xssl
* @param ciphertext  Data to decrypt
* @param len  Size of data to decrypt
* @param plaintext  Buffer to be filled with plaintext
* @param size  Size of plaintext buffer
*
* @return Number of plaintext bytes placed in buf on success
* @return 0 or <0 on failure
*/
int XSSL_decrypt(XSSL *xssl, const void *ciphertext, int len, void *plaintext, int size) {

	/* decrpyt the ciphertext */
	int plaintext_len = aes_decrypt(xssl->de,
							(unsigned char*)plaintext, size,
							(unsigned char*)ciphertext, len);	

	return plaintext_len;
}
