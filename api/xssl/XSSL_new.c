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
** @file XSSL_new.c
** @brief implements XSSL_new() and XSSL_free()
*/

#include "xssl.h"


/**
* @brief Create a new XSSL object.
*
* Create a new XSSL object, the primary data structure for an XSSL session.
* Among other things, this generates a temporary RSA keypair for this session.
*
* @param ctx An initialized XSSL_CTX.
*
* @return An initialized XSSL object on success, NULL on failure.
*/
XSSL *XSSL_new(XSSL_CTX *ctx) {
	XSSL *xssl = (XSSL*)malloc(sizeof(XSSL));

	xssl->ctx = ctx;
	xssl->session_keypair = RSA_generate_key(RSA_KEY_LENGTH, RSA_PUB_EXPONENT, NULL, NULL);

	return xssl;
}


/**
* @brief Frees a previously allocated XSSL object.
*
* @param xssl
*/
void XSSL_free(XSSL *xssl) {
	RSA_free(xssl->session_keypair);
	free(xssl->aes_key);
	free(xssl);
}
