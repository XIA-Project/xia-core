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
 @file Xgetaddrinfo.c
 @brief Implements Xgetgetaddrinfo(), Xfreeaddrinfo(), and Xgai_strerror()
*/
#include <errno.h>
#include <netdb.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"

/* FIXME: 
** - should we have XIA specific protocols for use in the addrinfo structure, or should it always be 0?
** - do something with the fallback flag
**
** philisophical questions:
** - is there an equivilent loopback address in XIA?
** - are there well know SIDs in XIA that should be findable in a getservbyname function?
** - are there multiple interfaces allowed in XIA?
** - are multiple AD/HIDs allowed on either the same interface, or different interfaces?
*/

//#define DAG_PLUS4ID   "RE ( %s ) %s %s"
//#define DAG_F_PLUS4ID "RE ( ( %s ) %s %s )"
//#define DAG 		  "RE %s %s"
//#define DAG_F 		  "RE ( %s %s )"
#define XID_LEN		  64


// XIA specific addrinfo error strings
const char *xerr_unimplemented = "This feature is not currently supported";



 const char *Xgai_strerror(int code)
 {
 	const char *msg = NULL;

 	switch (code) {
 		case XEAI_UNIMPLEMENTED:
 			msg = xerr_unimplemented;
 			break;

 		default:
 			msg = gai_strerror(code);
 	}

 	return msg;
 }



/*
** NOTE: although we currently check them, we don't use the protocol or socktype fields of the hints structure.
**  they are just checked to make sure no IPv4 code slips past us by mistake.
*/

int Xgetaddrinfo(const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **pai)
{
	struct addrinfo *ai = NULL;
	sockaddr_x sa;
	socklen_t slen;
	char sname[41];
	char stype[21];

	int local    = 0;	// local and loopback are the same right now, they tell us
	int loopback = 0;	//  to look up the local AD:HID instead of going to the name server
	int cname    = 0;	// if set, make ai_canonname pt to the name in the first sockaddr
	int havename = 0;	// user passed us a DAG instead of a name-string
	int haveserv = 0;	// user passed us an XID instead of a service-name
	int fallback = 0;	// user wants the dag wrapped in ()'s

	int socktype = 0;
	int protocol = 0;
	int flags    = 0;

	*pai = NULL;

	if (hints != NULL) {
		socktype = hints->ai_socktype;
		protocol = hints->ai_protocol;
		flags = hints->ai_flags;

		// make sure the user didn't ask for an IPv[4|6] address
		if (hints->ai_family != AF_XIA && hints->ai_family != AF_UNSPEC) {
			return EAI_FAMILY;
		}

		if (protocol != 0) {
			// XIA never sets a protocol
			return EAI_SOCKTYPE;
		}

		if (socktype != 0 && 
			socktype != XSOCK_STREAM && 
			socktype != XSOCK_DGRAM &&
			socktype != XSOCK_CHUNK) {
			// make sure it's one of our socket types. 
			// FIXME: should raw be allowed?
			return EAI_SOCKTYPE;
		}

		if (name == NULL) {
			// user wants up to look up the local AD:HID
			// currently we have the same behavior whether or not passive is set
			// this could change in the future
			if (flags & AI_PASSIVE) {
				local = 1;
			} else {
				loopback = 1;
			}
		}

		if (flags & AI_CANONNAME) {
			if (name == NULL) {
				// name can't be null if this is set as per normal getaddrinfo
				return EAI_BADFLAGS;
			}
			cname = 1;
		}

		if (flags & XAI_XIDSERV) {
			// same flag bit as AI_NUMERICSERV
			// we don't have ports, but if this is set, treat the service string
			//  as an XID and append it to the end of the DAG returned in the sockaddr

			if (!service)
				return EAI_BADFLAGS;

			if (!checkXid(service, NULL))
				return EAI_SERVICE;

			haveserv = 1;
		}

		if (flags & XAI_DAGHOST) {
			// same flags bit value as AI_NUMERICHOST
			// indicates that name is already a dag, so no lookup is needed

			if (!name)
				return EAI_BADFLAGS;

			// FIXME: insert code to validate that name conforms to a DAG
			havename = 1;
		}

		if (flags & AI_V4MAPPED) {
			// FIXME: will we ever want to use this flag?
			return EAI_BADFLAGS;
		}

		if (flags & AI_ADDRCONFIG) {
			// FIXME: will we ever want to use this flag?			
			return EAI_BADFLAGS;
		}

		if (flags & AI_ALL) {
			// FIXME: will we ever want to use this flag?
			return EAI_BADFLAGS;
		}

		// FIXME: at some point, add support for  international names
		if (flags & AI_IDN ||
			flags & AI_CANONIDN ||
			flags & AI_IDN_ALLOW_UNASSIGNED ||
			flags & AI_IDN_USE_STD3_ASCII_RULES) {

			return EAI_BADFLAGS;
		}

		if (flags & XAI_FALLBACK) {
			fallback = 1;
		}

	} else if (name == NULL) {
		// hints doesn't exist and NAME is null, so make loopback
		loopback = 1;
	}

	if (!haveserv && service) {
		// FIXME: implement service lookup
		//return XEAI_UNIMPLEMENTED;
	}


	if (service) {
		const char *p = strchr(service, ':');
		if (p) {
			bzero(stype, sizeof(stype));
			bzero(sname, sizeof(sname));
			strncpy(sname, p+1, 40);
			strncpy(stype, service, MIN(20, p - service));
		}
	}


	if (local || loopback) {
		// user wants the name of the local system for binding to a suocket to be used in accept calls

		/* NOTE: this code needs to be modified if we ever have the concept of INADDR_ANY or multiple interfaces or names for a host in XIA
		** at some point in the future local and loopback may require different code
		*/

		char ad[XID_LEN], hid[XID_LEN], fid[XID_LEN];
		int rc;
		int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);

		if (sock < 0) {
			return EAI_SYSTEM;
		}

		rc = XreadLocalHostAddr(sock, ad, XID_LEN, hid, XID_LEN, fid, XID_LEN);

		Xclose(sock);
		if (rc < 0) {
			return EAI_SYSTEM;
		}

		/* check the 4id and if it's correct, look at the last 8 chars to find the ip address
		** if they are 0.0.0.0 or 127.0.0.1, we can ignore the 4id as it's local only
		*/
		int have4id = 0;

		if (checkXid(fid, "IP:")) {
			// the IP address is in the last 8 bytes of the 4ID
			char *ip = fid + (strlen(fid) - 8);

			// FIXME: is the 127.0.0.1 check in the right byte order?
			if (strncmp(ip, "00000000", 8) != 0 &&
				strncasecmp(ip, "0100007f", 8) != 0) {
			
				have4id = 1;
			}
		}

		Node n_src = Node();
		Node n_ip  = Node(fid);
		Node n_ad  = Node(ad);
		Node n_hid = Node(hid);

		Graph g = (n_src * n_ad * n_hid);

		if (have4id)
			g = g + (n_src * n_ip * n_ad * n_hid);

		if (fallback) {
			Graph gf = n_src;
			g = gf + g;
		}

		if (service)
			g = g * Node(stype, sname);

		g.fill_sockaddr(&sa);

	} else if (!havename) {
		slen = sizeof(sa);
		if (XgetDAGbyName(name, &sa, &slen) < 0) {
			return EAI_NONAME;
		}

		if (service) {
			Graph g(&sa);

			if (fallback) {
				Graph gf = Node();
				g = gf + g;
			}
			g = g * Node(stype, sname);
			g.fill_sockaddr(&sa);
		}

	} else {
		// caller wants us to use name as the returned dag
		Graph g(name);

		if (fallback) {
			Graph gf = Node();
			g = gf + g;
		}

		if (service)
			g = g * Node(stype, sname);

		g.fill_sockaddr(&sa);
	}

	// allocate memory needed
 	ai = (struct addrinfo *)calloc(sizeof(struct addrinfo), 1);
 	sockaddr_x *psa = (sockaddr_x *)calloc(sizeof(sockaddr_x), 1);
	if (!ai || !psa) {
		if (ai)
			free(ai);
		return EAI_MEMORY;
	}
	memcpy(psa, &sa, sizeof(sa));

	// fill in the blanks

	ai->ai_family    = AF_XIA;
	ai->ai_socktype  = socktype;
	ai->ai_protocol  = protocol;
	ai->ai_flags     = 0;
	ai->ai_addr      = (struct sockaddr *)psa;
	ai->ai_addrlen   = sizeof(sockaddr_x);
	ai->ai_next      = NULL;	// this could change in the future at least for local lookups if multiple interfaces are present

	if (cname) {
		// FIXME: should we allocate a copy of the dag string in this case?
		ai->ai_canonname = NULL;
	}

	*pai = ai;

	return 0;
}

void Xfreeaddrinfo(struct addrinfo *ai)
{
	// no xia specific processing required at the moment
	freeaddrinfo(ai);
}
