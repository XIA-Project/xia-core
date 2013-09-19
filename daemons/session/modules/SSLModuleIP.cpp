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

#include "SSLModule.h"
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

bool SSLModuleIP::preSend(struct breakpoint_context *context, void *rv) {
	
	struct send_args *args = (struct send_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	const char *buf = args->buf;
	int len = *(args->len);

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
		*(int*)rv = SSL_write(*(SSL**)cinfo->ssl_ptr().data(), buf, len);
		return false;
	} else {
		return true;
	}
}

bool SSLModuleIP::preRecv(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
		// cancel the recv() call since we'll do a read ourselves
		return false;
	}

	return true;
}

bool SSLModuleIP::postRecv(struct breakpoint_context *context, void *rv) {
	
	struct recv_args *args = (struct recv_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	char *buf = args->buf;
	int len = *(args->len);

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
		*(int*)rv = SSL_read(*(SSL**)cinfo->ssl_ptr().data(), buf, len);
	}

	return true;
}

bool SSLModuleIP::postRecvSYN(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;
	if (cinfo->use_ssl()) { 
		// If we're accpeting, we should have already bound, which set the ssl ctx
		if (!cinfo->has_ssl_ctx_ptr()) {
			ERROR("Connection does not have an associated SSL_ctx");
			return true;
		}
		
		// Make sure we haven't already accepted
		if (cinfo->has_ssl_ptr()) return true;

		SSL *ssl = SSL_new(*(SSL_CTX**)cinfo->ssl_ctx_ptr().data());
		if (ssl == NULL) {
			ERROR("Unable to init new SSL object");
			return true;
		}
		if (SSL_set_fd(ssl, cinfo->sockfd()) != 1) {
			ERROR("Unable to set SSL sockfd");
			return true;
		}
		if (SSL_accept(ssl) != 1) {
			ERRORF("Error accepting SSL connection: %s", ERR_error_string(ERR_get_error(), NULL));
			exit(-1);
			return true;
		}
		cinfo->set_ssl_ptr(&ssl, sizeof(SSL*));
	}

	return true;
}

bool SSLModuleIP::postSendSYN(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;
	if (cinfo->use_ssl()) {
		if (!cinfo->has_ssl_ctx_ptr()) {
			SSL_CTX *ssl_ctx = SSL_CTX_new(SSLv23_client_method());
			if (ssl_ctx == NULL) {
				ERROR("Unable to init new SSL context");
				return true;
			}
			cinfo->set_ssl_ctx_ptr(&ssl_ctx, sizeof(SSL_CTX*));

			// set private key
			SSL_CTX_use_RSAPrivateKey(ssl_ctx, RSA_generate_key(1024, 3, NULL, NULL));
			SSL_CTX_set_cipher_list(ssl_ctx, "aNULL");
		}

		// Make sure we haven't already connected
		if (cinfo->has_ssl_ptr()) return true;

		SSL *ssl = SSL_new(*(SSL_CTX**)cinfo->ssl_ctx_ptr().data());
		if (ssl == NULL) {
			ERROR("Unable to init new SSL object");
			return true;
		}
		if (SSL_set_fd(ssl, cinfo->sockfd()) != 1) {
			ERROR("Unable to set SSL sockfd");
			return true;
		}
		int rc;
		if ((rc = SSL_connect(ssl)) != 1) {
			ERRORF("Unable to initiate SSL connection: %s", ERR_error_string(ERR_get_error(), NULL));
			exit(-1);
			return true;
		}
		cinfo->set_ssl_ptr(&ssl, sizeof(SSL*));
	}

	return true;
}
