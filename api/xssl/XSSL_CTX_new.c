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
** @file XSSL_CTX_new.c
** @brief implements XSSL_CTX_new() and XSSL_CTX_free()
*/

#include "xssl.h"


/**
* @brief Create a new XSSL_CTX object and initialize with a newly generated
*	RSA key pair.
*
* @return A pointer to an XSSL_CTX object. Caller is responsible for freeing
*	it with XSSL_CTX_free().
*/
XSSL_CTX *XSSL_CTX_new() {
	XSSL_CTX *ctx = (XSSL_CTX*)malloc(sizeof(XSSL_CTX));
    ctx->keypair = RSA_generate_key(RSA_KEY_LENGTH, RSA_PUB_EXPONENT, NULL, NULL);
	return ctx;
}



/**
* @brief Frees an XSSL_CTX object.
*
* @param ctx
*/
void XSSL_CTX_free(XSSL_CTX *ctx) {
	RSA_free(ctx->keypair);
	free(ctx);
}
