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

#include <time.h>
#include "InterfaceModule.h"

void InterfaceModule::decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
										 AppLayerInfo &appInfo, 
										 TransportLayerInfo &transportInfo, 
										 NetworkLayerInfo &netInfo, 
										 LinkLayerInfo &linkInfo, 
										 PhysicalLayerInfo &physInfo) {
	
	(void)transportInfo;
	(void)netInfo;

	srand(time(NULL));
	vector<string> ifaces = linkInfo.getActiveInterfaces();

	//// for debugging, print list of interfaces
	//string iface_string = "";
	//for (vector<string>::iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
	//	iface_string += *it + "  ";
	//}
	//DBGF("Active interfaces: %s", iface_string.c_str());


	// Simulate removing 3G from the list if battery level is too low
	float batteryLevel = physInfo.getDeviceBatteryLevel();
	DBGF("Current battery: %f", batteryLevel);
	if ( batteryLevel < userInfo.getCellularBatteryCutoff()) {
		DBG("Eliminating 3G");
	}


	// Simulate using highest BW or lowest latency interface
	if ( appInfo.checkAttribute(kBandwidthPriority) ) {

		float bestBW = 0;
		string bestIface = "";
		for (vector<string>::iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
			float bw = linkInfo.getBandwidthForInterface(*it);
			if (bw > bestBW) {
				bestBW = bw;
				bestIface = *it;
			}
		}
		
		sinfo->set_interface(bestIface);

	} else if ( appInfo.checkAttribute(kLatencyPriority) ) {
		
		float bestLat = 1000;
		string bestIface = "";
		for (vector<string>::iterator it = ifaces.begin(); it != ifaces.end(); ++it) {
			float lat = linkInfo.getLatencyForInterface(*it);
			if (lat < bestLat) {
				bestLat = lat;
				bestIface = *it;
			}
		}
		
		sinfo->set_interface(bestIface);

	} else {
		// just use the first one for now
		sinfo->set_interface(ifaces[0]);
	}

	DBGF("Using interface: %s", sinfo->interface().c_str());
}

bool InterfaceModule::breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) {
	(void)breakpoint;
	(void)context;
	(void)rv;
	return true;
}
