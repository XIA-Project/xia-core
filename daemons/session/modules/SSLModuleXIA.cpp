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

void SSLModuleXIA::decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
										 AppLayerInfo &appInfo, 
										 TransportLayerInfo &transportInfo, 
										 NetworkLayerInfo &netInfo, 
										 LinkLayerInfo &linkInfo, 
										 PhysicalLayerInfo &physInfo) {
	
	(void)transportInfo;
	(void)netInfo;
	(void)linkInfo;
	(void)physInfo;

	if (userInfo.getEncrypt() || appInfo.checkAttribute(kEncryption)) {
		DBG("Using SSL");
		sinfo->set_use_ssl(true);
	} else {
		DBG("Not using SSL");
		sinfo->set_use_ssl(false);
	}
}

bool SSLModuleXIA::breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) {

	switch(breakpoint) {
		case kSendPreSend:
			DBG("kSendPreSend");
			return preSend(context, rv);
		case kRecvPreRecv:
			DBG("kRecvPreRecv");
			return preRecv(context, rv);
		case kRecvPostRecv:
			DBG("kRecvPostRecv");
			return postRecv(context, rv);
		case kAcceptPostReceiveSYN:
			DBG("kAcceptPostReceiveSYN");
			return postRecvSYN(context, rv);
		default:
			DBG("Other breakpoint");
			return true;
	}
}

bool SSLModule::preSend(struct breakpoint_context *context, void *rv) {
	
	struct send_args *args = (struct send_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	const char *buf = args->buf;
	int len = *(args->len);

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
	    *(int*)rv = XSSL_write(*(XSSL**)cinfo->ssl_ptr().data(), buf, len);
		return false;
	} else {
		return true;
	}
}

bool SSLModule::preRecv(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
		// cancel the recv() call since we'll do a read ourselves
		return false;
	}

	return true;
}

bool SSLModule::postRecv(struct breakpoint_context *context, void *rv) {
	
	struct recv_args *args = (struct recv_args*)context->args;
	session::ConnectionInfo *cinfo = context->cinfo;
	char *buf = args->buf;
	int len = *(args->len);

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
		*(int*)rv = XSSL_read(*(XSSL**)cinfo->ssl_ptr().data(), buf, len);
	}

	return true;
}

bool SSLModule::postRecvSYN(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	session::ConnectionInfo *cinfo = context->cinfo;
	if (cinfo->use_ssl()) { 
	
		// If we're accpeting, we should have already bound, which set the ssl ctx
		if (!cinfo->has_ssl_ctx_ptr()) {
			ERROR("Connection does not have an associated XSSL_ctx");
		}
		
		// Make sure we haven't already accepted
		if (cinfo->has_ssl_ptr()) return 0;

		XSSL *xssl = XSSL_new(*(XSSL_CTX**)cinfo->ssl_ctx_ptr().data());
		if (xssl == NULL) {
			ERROR("Unable to init new XSSL object");
		}
		if (XSSL_set_fd(xssl, cinfo->sockfd()) != 1) {
			ERROR("Unable to set XSSL sockfd");
		}
		if (XSSL_accept(xssl) != 1) {
			ERROR("Error accepting XSSL connection");
		}
		cinfo->set_ssl_ptr(&xssl, sizeof(XSSL*));
	}

	return true;
}
