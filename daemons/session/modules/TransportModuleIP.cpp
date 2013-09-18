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

#include "TransportModule.h"
#include "session.h"


void TransportModuleIP::decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
										 AppLayerInfo &appInfo, 
										 TransportLayerInfo &transportInfo, 
										 NetworkLayerInfo &netInfo, 
										 LinkLayerInfo &linkInfo, 
										 PhysicalLayerInfo &physInfo) {

	(void)userInfo;
	(void)transportInfo;
	(void)netInfo;
	(void)linkInfo;
	(void)physInfo;

	if (appInfo.checkAttribute(kReliableDelivery)) {
		sinfo->set_transport_protocol(session::TCP);
	} else {
		sinfo->set_transport_protocol(session::UDP);
	}

}

bool TransportModuleIP::breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) {
	return true;
}
