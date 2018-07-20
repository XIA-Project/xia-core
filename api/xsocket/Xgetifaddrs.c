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
 @brief Xgetifaddrs(), Xfreeifaddrs() - get interface addresses
*/
#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include <netdb.h>
#include "minIni.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
#include <stdlib.h>
#include <sys/types.h>
#include <ifaddrs.h>
/*! \endcond */

static int add_ifaddr(struct ifaddrs **ifap, const xia::X_GetIfAddrs_Msg::IfAddr& ifaddr)
{
	int retval = -1;
	int state = 0;

	//printf("Creating a new ifaddr\n");
	// Create a new struct ifaddrs
	struct ifaddrs *_ifaddr = (struct ifaddrs*) calloc(sizeof(struct ifaddrs), 1);
	if(!_ifaddr) {
		LOG("Unable to allocate memory to hold ifaddr");
		goto add_ifaddr_done;
	}
	state = 1;    // _ifaddr needs to be freed

	// Will push to end of queue, so ifa_next will be NULL
	_ifaddr->ifa_next = NULL;

	// Store interface name
	_ifaddr->ifa_name = (char *) calloc(ifaddr.iface_name().size()+1, 1);
	if(!_ifaddr->ifa_name) {
		LOG("Unable to allocate memory to hold interface name");
		goto add_ifaddr_done;
	}
	state = 2;    // _ifaddr->ifa_name needs freeing
	strcpy(_ifaddr->ifa_name, ifaddr.iface_name().c_str());
	//printf("Adding interface %s\n", ifaddr.iface_name().c_str());

	// Store flags
	_ifaddr->ifa_flags = ifaddr.flags();
	if(ifaddr.is_rv_dag()) {
		_ifaddr->ifa_flags |= XIFA_RVDAG;
	}

	// Store interface DAG
	_ifaddr->ifa_addr = (struct sockaddr *) calloc(sizeof(sockaddr_x), 1);
	if(!_ifaddr->ifa_addr) {
		LOG("Unable to allocate memory to hold interface address");
		goto add_ifaddr_done;
	}
	state = 3;    // _ifaddr->ifa_addr needs freeing
	{
		Graph gs(ifaddr.src_addr_str().c_str());
		gs.fill_sockaddr((sockaddr_x *)_ifaddr->ifa_addr);
		//printf("with address: %s\n", ifaddr.src_addr_str().c_str());
	}

	// Netmask is always NULL for XIA
	_ifaddr->ifa_netmask = NULL;

	// Store destination DAG that other can use to reach this interface
	_ifaddr->ifa_dstaddr = (struct sockaddr *) calloc(sizeof(sockaddr_x), 1);
	if(!_ifaddr->ifa_dstaddr) {
		goto add_ifaddr_done;
	}
	state = 4;    // _ifaddr->ifa_dstaddr needs freeing
	{
		//printf("with dst_addr: %s\n", ifaddr.dst_addr_str().c_str());
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

/*!
** @brief get a list of XIA network interfaces
**
** The Xgetifaddrs() function creates a linked list of structures describing
** XIA enabled network interfaces of the local system, and stores the address
** of the first item of the list in *ifap. The list consists of ifaddrs
** structures, defined as follows:
**
\verbatim
 struct ifaddrs {
		struct ifaddrs  *ifa_next;    // Next item in list
		char            *ifa_name;    // Name of interface
		unsigned int     ifa_flags;   // Flags from SIOCGIFFLAGS
		struct sockaddr *ifa_addr;    // Address of interface
		struct sockaddr *ifa_netmask; // Netmask of interface
		union {
			struct sockaddr *ifu_broadaddr;
			// Broadcast address of interface
			struct sockaddr *ifu_dstaddr;
			// Point-to-point destination address
		} ifa_ifu;
		#define          ifa_broadaddr ifa_ifu.ifu_broadaddr
		#define          ifa_dstaddr   ifa_ifu.ifu_dstaddr
		void            *ifa_data;    // Address-specific data
	};
\endverbatim
**
** The ifa_next field contains a pointer to the next structure on the list, or
** NULL if this is the last item of the list.
**
** The ifa_name points to the null-terminated interface name.
**
** The ifa_flags field contains the interface flags, as returned by the
** SIOCGIFFLAGS ioctl(2) operation (see netdevice(7) for a list of these flags).
**
** The ifa_addr field points to a structure containing the interface address.
** Under XIA, these will all be of type sockaddr_x. This field may contain a null pointer.
**
** The ifa_netmask field points will always be NULL under XIA.
**
** Currently the ifa_ifu union will never contain a broadcast address.
**
** The ifa_data pointer will always be NULL
**
** The data returned by Xgetifaddrs() is dynamically allocated and should be
** freed using Xfreeifaddrs() when no longer needed.
**
** @param ifap pointer to a pointer to accept the returned ifaddrs structure
**
** @returns 0 on success
** @returns -1 on error with errno set appropriately.
*/
int Xgetifaddrs(struct ifaddrs **ifap)
{
	// TODO: Set errno for various errors
	int rc;

	// A socket that Click can associate us with
	int sockfd = MakeApiSocket(SOCK_DGRAM);
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

	//printf("Xgetifaddrs: sending request to Click\n");
	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		Xclose(sockfd);
		return -1;
	}

	//printf("Xgetifaddrs: waiting for Click response\n");
	xia::XSocketMsg xsm1;
	if ((rc = click_reply(sockfd, seq, &xsm1)) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		Xclose(sockfd);
		return -1;
	}

	//printf("Xgetifaddrs: converting Click response to struct ifaddrs\n");
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

	freeSocketState(sockfd);
	(_f_close)(sockfd);
	return rc;
}

/*!
** @brief free memory allocated by Xgetifaddrs
**
** Frees the memory allocated by XgetifAddrs().
**
** @param ifa pointer to ifaddr structure to be freed.
**
** @returns void
*/
void Xfreeifaddrs(struct ifaddrs *ifa)
{
	struct ifaddrs *p;
	while(ifa != NULL) {
		p = ifa;
		ifa = ifa->ifa_next;
		if(p->ifa_name) {
			free(p->ifa_name);
		}
		if(p->ifa_addr) {
			free(p->ifa_addr);
		}
		if(p->ifa_dstaddr) {
			free(p->ifa_dstaddr);
		}
		free(p);
	}
}
