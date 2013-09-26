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
#include "xssl.h"

bool SSLModuleHybrid::preSend(struct breakpoint_context *context, void *rv) {
	(void)rv;

	struct send_args *args = (struct send_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	char *buf = args->buf;
	int msglen = *(args->msglen);
	int buflen = args->buflen;

	if (cinfo == NULL) {
		WARN("Connection info was null; defaulting to no encryption");
		return true;
	}

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
	    //*(int*)rv =???;
		unsigned char temp[65000];
		memcpy(temp, buf, msglen);
	    int encrypted_len = XSSL_encrypt(*(XSSL**)cinfo->ssl_ptr().data(), temp, msglen, buf, buflen);

		if (encrypted_len <= 0) {
			ERROR("Error encrypting data");
			return true;
		}

		*(args->msglen) = encrypted_len; // tell future blocks the new message length
		DBGF("Encrypted %d bytes to %d bytes", msglen, encrypted_len);
	}
		
	return true;
}

bool SSLModuleHybrid::postRecv(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	struct recv_args *args = (struct recv_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	char *buf = args->buf;
	int msglen = *(args->msglen);
	int buflen = args->buflen;

	if (cinfo == NULL) {
		WARN("Connection info was null; defaulting to no encryption");
		return true;
	}

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
	    //*(int*)rv =???;
		unsigned char temp[65000];
		memcpy(temp, buf, msglen);
		int decrypted_len = XSSL_decrypt(*(XSSL**)cinfo->ssl_ptr().data(), temp, msglen, buf, buflen);

		if (decrypted_len <= 0) {
			ERROR("Error decrypting data");
			return true;
		}

		*(args->msglen) = decrypted_len; // tell future blocks the new message length
		DBGF("Decrypted %d bytes to %d bytes", msglen, decrypted_len);
	}
		
	return true;
}

bool SSLModuleHybrid::postRecvSYN(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;
	if (cinfo->use_ssl()) { 
	
		// If we're accpeting, we should have already bound, which set the ssl ctx
		if (!cinfo->has_ssl_ctx_ptr()) {
			ERROR("Connection does not have an associated XSSL_ctx");
			return true;
		}
		
		// Make sure we haven't already accepted
		if (cinfo->has_ssl_ptr()) return 0;

		XSSL *xssl = XSSL_new(*(XSSL_CTX**)cinfo->ssl_ctx_ptr().data());
		if (xssl == NULL) {
			ERROR("Unable to init new XSSL object");
			return true;
		}
		if (XSSL_set_fd(xssl, cinfo->sockfd()) != 1) {
			ERROR("Unable to set XSSL sockfd");
			return true;
		}
		if (XSSL_accept_ip(xssl) != 1) {
			ERROR("Error accepting XSSL connection");
			return true;
		}
		cinfo->set_ssl_ptr(&xssl, sizeof(XSSL*));
	}

	return true;
}

bool SSLModuleHybrid::postSendSYN(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;
	if (cinfo->use_ssl()) {
		if (!cinfo->has_ssl_ctx_ptr()) {
			XSSL_CTX *xssl_ctx = XSSL_CTX_new();
			if (xssl_ctx == NULL) {
				ERROR("Unable to init new XSSL context");
				return true;
			}
			cinfo->set_ssl_ctx_ptr(&xssl_ctx, sizeof(XSSL_CTX*));
		}

		// Make sure we haven't already connected
		if (cinfo->has_ssl_ptr()) return true;

		XSSL *xssl = XSSL_new(*(XSSL_CTX**)cinfo->ssl_ctx_ptr().data());
		if (xssl == NULL) {
			ERROR("Unable to init new XSSL object");
			return true;
		}
		if (XSSL_set_fd(xssl, cinfo->sockfd()) != 1) {
			ERROR("Unable to set XSSL sockfd");
			return true;
		}
		if (XSSL_connect_ip(xssl) != 1) {
			ERROR("Unable to initiatie XSSL connection");
			return true;
		}
		cinfo->set_ssl_ptr(&xssl, sizeof(XSSL*));
	}

	return true;
}
