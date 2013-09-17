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

bool SSLModuleXIA::breakpoint(Breakpoint breakpoint, void *context, void *rv) {

	switch(breakpoint) {
		case kSendPreSend:
			DBG("kSendPreSend");
			return preSend(context, rv);
		default:
			DBG("Other breakpoint");
			return true;
	}
}

bool SSLModule::preSend(void *context, void *rv) {
	
	struct breakpoint_context *bctx = (struct breakpoint_context*)context;
	session::ConnectionInfo *cinfo = bctx->cinfo;
	const char *buf = bctx->send_buf;
	int len = bctx->len;

	if (cinfo->use_ssl() && cinfo->has_ssl_ptr()) {
	    *(int*)rv = XSSL_write(*(XSSL**)cinfo->ssl_ptr().data(), buf, len);
		return false;
	} else {
		return true;
	}
}
