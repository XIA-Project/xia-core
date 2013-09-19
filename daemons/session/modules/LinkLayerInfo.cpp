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

#include "LinkLayerInfo.h"
#include <ifaddrs.h>


vector<string> LinkLayerInfo::getActiveInterfaces() {

	struct ifaddrs *ifaddr, *ifa;
	vector<string> ifaces;
	
	if (getifaddrs(&ifaddr) == -1) {
	    ERROR("getifaddrs");
		return ifaces;
	}
	
	/* Walk through linked list, maintaining head pointer so we
	   can free list later */
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

	    if (ifa->ifa_addr != NULL)  // skip inactive interfaces
			if (ifa->ifa_addr->sa_family == AF_INET) // hack to avoid duplicate entries
				ifaces.push_back(string(ifa->ifa_name));
	}
	
	freeifaddrs(ifaddr);
	return ifaces;
}

vector<string> LinkLayerInfo::getAllInterfaces() {

	struct ifaddrs *ifaddr, *ifa;
	vector<string> ifaces;
	
	if (getifaddrs(&ifaddr) == -1) {
	    ERROR("getifaddrs");
		return ifaces;
	}
	
	/* Walk through linked list, maintaining head pointer so we
	   can free list later */
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

		if (ifa->ifa_addr->sa_family == AF_INET) // hack to avoid duplicate entries
			ifaces.push_back(string(ifa->ifa_name));
	}
	
	freeifaddrs(ifaddr);
	return ifaces;
}
		
float LinkLayerInfo::getBandwidthForInterface(string iface) {
	return rand() % 150; // random BW up to 150 Mbps
}

float LinkLayerInfo::getLatencyForInterface(string iface) {
	return rand() % 100; // random latency up to 100 ms
}
