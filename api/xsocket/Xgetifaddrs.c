/* ts=4 */
/*
** Copyright 2012 Carnegie Mellon University
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
 @file Xgetifaddrs.c
 @brief Implements Xgetifaddrs(), Xfreeifaddrs()
*/
#include <errno.h>
#include <netdb.h>
#include "minIni.h"
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
#include <stdlib.h>
#include <sys/types.h>
#include <ifaddrs.h>

int add_ifaddr(struct ifaddrs **ifap, const xia::X_GetIfAddrs_Msg::IfAddr& ifaddr)
{
	int retval = -1;
	int state = 0;

	// Create a new struct ifaddrs
	struct ifaddrs *_ifaddr = (struct ifaddrs*) calloc(sizeof(struct ifaddrs), 1);
	if(!_ifaddr) {
		goto add_ifaddr_done;
	}
	state = 1;    // _ifaddr needs to be freed

	// Will push to end of queue, so ifa_next will be NULL
	_ifaddr->ifa_next = NULL;

	// Store interface name
	_ifaddr->ifa_name = (char *) calloc(ifaddr.iface_name().size()+1, 1);
	if(!_ifaddr->ifa_name) {
		goto add_ifaddr_done;
	}
	state = 2;    // _ifaddr->ifa_name needs freeing
	strcpy(_ifaddr->ifa_name, ifaddr.iface_name().c_str());

	// Store flags
	_ifaddr->ifa_flags = ifaddr.flags();

	// Store interface DAG
	_ifaddr->ifa_addr = (struct sockaddr *) calloc(sizeof(sockaddr_x), 1);
	if(!_ifaddr->ifa_addr) {
		goto add_ifaddr_done;
	}
	state = 3;
	{
		Graph gs(ifaddr.src_addr_str().c_str());
		gs.fill_sockaddr((sockaddr_x *)_ifaddr->ifa_addr);
	}

	// Netmask is always NULL for XIA
	_ifaddr->ifa_netmask = NULL;

	// Store destination DAG that other can use to reach this interface
	_ifaddr->ifa_dstaddr = (struct sockaddr *) calloc(sizeof(sockaddr_x), 1);
	if(!_ifaddr->ifa_dstaddr) {
		goto add_ifaddr_done;
	}
	state = 4;
	{
		Graph gd(ifaddr.dst_addr_str().c_str());
		gd.fill_sockaddr((sockaddr_x *)_ifaddr->ifa_dstaddr);
	}

	// ifa_data will be NULL for XIA
	_ifaddr->ifa_data = NULL;

	// First element in list
	if(*ifap == NULL) {
		*ifap = _ifaddr;
	} else {
		// Walk to end of list and append
		struct ifaddrs *this_ifaddr = *ifap;
		while(this_ifaddr->ifa_next != NULL) {
			this_ifaddr = this_ifaddr->ifa_next;
		}
		this_ifaddr->ifa_next = _ifaddr;
	}
	// All went well, return success
	retval = 0;
	return retval;

	// We ran into some problem, clean up allocated memory
add_ifaddr_done:
	switch(state) {
		case 4:
			free(_ifaddr->ifa_dstaddr);
		case 3:
			free(_ifaddr->ifa_addr);
		case 2:
			free(_ifaddr->ifa_name);
		case 1:
			free(_ifaddr);
	}
	return retval;
}

// TODO: Set errno for various errors
int Xgetifaddrs(struct ifaddrs **ifap)
{
	int rc;

	// A socket that Click can associate us with
	int sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		LOG("Xgetifaddrs: unable to create Xsocket");
		return -1;
	}

	// Initialize list of ifaddrs to NULL
	*ifap = NULL;

	// Send a request to Click to read info on all interfaces
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETIFADDRS);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

    if ((rc = click_send(sockfd, &xsm)) < 0) {
        LOGF("Error talking to Click: %s", strerror(errno));
        return -1;
    }

    xia::XSocketMsg xsm1;
    if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
        LOGF("Error retrieving status from Click: %s", strerror(errno));
        return -1;
    }

	// Send list of interfaces up to the application
    if (xsm1.type() == xia::XGETIFADDRS) {
        xia::X_GetIfAddrs_Msg *msg = xsm1.mutable_x_getifaddrs();
		for(int i=0; i<msg->ifaddrs_size(); i++) {
			if(add_ifaddr(ifap, msg->ifaddrs(i))) {
				LOG("Error adding ifaddr for an ifaddrs entry from Click");
			}
		}
        rc = 0;
    } else {
        LOG("Xgetifaddrs: ERROR: Invalid response for XGETIFADDRS request");
        rc = -1;
    }
	return rc;
}

void Xfreeifaddrs(struct ifaddrs *ifa)
{
	// no xia specific processing required at the moment
	freeifaddrs(ifa);
}
