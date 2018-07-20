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
 @brief Xgetaddrinfo(), Xfreeaddrinfo(), Xgai_strerror() - network address and service translation
*/

#include "Xsocket.h"
/*! \cond */
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include "minIni.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"


#if 0


typedef struct {
	unsigned int  s_type;
	unsigned char s_id[XID_SIZE];
} xid_t;

typedef struct {
	xid_t         s_xid;
	unsigned char s_edge[EDGES_MAX];
} node_t;

typedef struct click_xia_xid xid_t;

typedef struct click_xia_xid_node node_t;

typedef struct {
	unsigned char s_count;
	node_t        s_addr[NODES_MAX];
} x_addr_t;



typedef struct {
	// common sockaddr fields
#ifdef __APPLE__
	unsigned char sx_len; // not actually large enough for sizeof(sockaddr_x)
	unsigned char sx_family;
#else
	unsigned short sx_family;
#endif

	// XIA specific fields
	x_addr_t      sx_addr;
} sockaddr_x;


/*! @struct sockaddr_x
 *  @brief This structure blah blah blah...
 *  @var foreignstruct::a
 *  Member 'a' contains...
 *  @var foreignstruct::b
 *  Member 'b' contains...
 */

#endif
/* FIXME:
** - should we have XIA specific protocols for use in the addrinfo structure, or should it always be 0?
** - do something with the fallback flag
**
** philisophical questions:
** - is there an equivilent loopback address in XIA?
** - are there well know SIDs in XIA that should be findable in a getservbyname function?
*/

//#define DAG_PLUS4ID   "RE ( %s ) %s %s"
//#define DAG_F_PLUS4ID "RE ( ( %s ) %s %s )"
//#define DAG 		  "RE %s %s"
//#define DAG_F 		  "RE ( %s %s )"
#define XID_LEN		  64

#define CONFIG_PATH_BUF_SIZE 1024
#define RESOLV_CONF "/etc/resolv.conf"
/*! \endcond */

/*! struct xid_t
** @brief XID definition
** @var xid_t::s_type
** @var xid_t::s_id
*/


// XIA specific addrinfo error strings
static const char *xerr_unimplemented = "This feature is not currently supported";


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


/*!
 * Read DAG for rendezvous server data plane from resolv.conf
 *
 * The XreadRVServerAddr() API will read the rendezvous service address
 * from etc/resolv.conf file. The caller must be expecting the address
 * to be available in the config file. This API does not call into the
 * XIA stack and is simply a way to read the config file.
 *
 * @param rv_dag_str a buffer to be filled in
 * @param rvstrlen the size of the rv_dag_str buffer
 *
 * @returns 0 on success, -1 on failure
 */
int XreadRVServerAddr(char *rv_dag_str, int rvstrlen)
{
	int ret = -1;
	int rc;

	// If there is a resolv.conf file with rendezvous info, return that info
	char root[CONFIG_PATH_BUF_SIZE];

	// clear out the buffers
	bzero(rv_dag_str, rvstrlen);
	bzero(root, CONFIG_PATH_BUF_SIZE);

	// Read Rendezvous server DAG from xia-core/etc/resolv.conf, if present
	rc = ini_gets(NULL, "rendezvous", rv_dag_str, rv_dag_str, rvstrlen,
			strcat(XrootDir(root, CONFIG_PATH_BUF_SIZE), RESOLV_CONF));
	if(rc > 0) {
		if (rc < rvstrlen) {
			ret = 0;
		} else {
			printf("XreadRVServerAddr: ERROR: %d byte buffer", rvstrlen);
			printf(" for %d byte RV DAG\n", rc);
		}
	} else {
		printf("XreadRVServerAddr: Rendezvous server DAG not found:%s:\n",
				rv_dag_str);
	}
	return ret;
}

/*!
 * Read DAG for rendezvous server control plane from resolv.conf
 *
 * The XreadRVServecControlAddr reads the control-plane address for the
 * rendezvous service running on this system from etc/resolv.conf. The
 * caller must be expecting the address to be available in the config
 * file. This API does not call into the XIA stack and is simply a way
 * to read the config file.
 *
 * @param rv_dag_str a buffer to return the control address in
 * @param rvstrlen the size of rv_dag_str buffer
 *
 * @returns 0 on success, -1 on failure
 */
int XreadRVServerControlAddr(char *rv_dag_str, int rvstrlen)
{
	int ret = -1;
	int rc;

	// If there is a resolv.conf file with rendezvous info, return that info
	char root[CONFIG_PATH_BUF_SIZE];

	// clear out the buffers
	bzero(rv_dag_str, rvstrlen);
	bzero(root, CONFIG_PATH_BUF_SIZE);

	// Read Rendezvous server DAG from xia-core/etc/resolv.conf, if present
	rc = ini_gets(NULL, "rendezvousc", rv_dag_str, rv_dag_str, rvstrlen,
			strcat(XrootDir(root, CONFIG_PATH_BUF_SIZE), RESOLV_CONF));
	if(rc > 0) {
		printf("XreadRVServerControlAddr: found server at:%s:\n", rv_dag_str);
		ret = 0;
	}
	return ret;
}

static int _append_addrinfo(struct addrinfo **pai, sockaddr_x sa, int socktype, int protocol, int cname)
{
	struct addrinfo *ai = NULL;
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
	ai->ai_next      = NULL;	// this addrinfo will be added to end of list

	if (cname) {
		// FIXME: should we allocate a copy of the dag string in this case?
		ai->ai_canonname = NULL;
	}

	// If the list is empty, simply add this addrinfo and return
	if(*pai == NULL) {
		*pai = ai;
		return 0;
	}
	// If the list was not empty, walk to the end of the list
	struct addrinfo *listptr = *pai;
	while(listptr->ai_next != NULL) {
		listptr = listptr->ai_next;
	}
	// Append this addrinfo as the next element in the list
	listptr->ai_next = ai;
	return 0;
}


/*!
** @brief Get the full DAG of the remote socket.
**
** \todo flesh out Xgetaddrinfo() text
**
** @param sockfd An Xsocket of type SOCK_STREAM
** @param dag A sockaddr to hold the returned DAG.
** @param len On input contans the size of the sockaddr
**  on output contains sizeof(sockaddr_x).
**
** @returns 0 on success
** @returns -1 on failure with errno set
** @returns errno = EFAULT if dag is NULL
** @returns errno = EOPNOTSUPP if sockfd is not of type XSSOCK_STREAM
** @returns errno = ENOTCONN if sockfd is not in a connected state
**
*/
int Xgetaddrinfo(const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **pai)
{
	/*
	** NOTE: although we currently check them, we don't use the protocol or socktype fields of the hints structure.
	**  they are just checked to make sure no IPv4 code slips past us by mistake.
	*/
	int rc;
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
			socktype != SOCK_STREAM &&
			socktype != SOCK_DGRAM) {
			// make sure it's one of our socket types.
			// FIXME: should raw be allowed?
			return EAI_SOCKTYPE;
		}

		if (name == NULL || strlen(name) == 0) {
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
#ifdef AI_IDN
		// FIXME: at some point, add support for  international names
		if (flags & AI_IDN ||
			flags & AI_CANONIDN ||
			flags & AI_IDN_ALLOW_UNASSIGNED ||
			flags & AI_IDN_USE_STD3_ASCII_RULES) {

			return EAI_BADFLAGS;
		}
#endif
		if (flags & XAI_FALLBACK) {
			fallback = 1;
		}

	} else if (name == NULL || strlen(name) == 0) {
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
		// New multi-homing implementation, return DAGs for all interfaces
//		int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
//		if (sock < 0) {
//			return EAI_SYSTEM;
//		}
		// Read DAGs for all system interfaces
		struct ifaddrs *ifaddr, *ifa;
		if(Xgetifaddrs(&ifaddr)) {
			return EAI_SYSTEM;
		}
		// Add a DAG for each interface to the results in pai
		for(ifa=ifaddr; ifa!=NULL; ifa=ifa->ifa_next) {
			sockaddr_x *iface_dag = (sockaddr_x *)ifa->ifa_addr;
			Graph gif(iface_dag);
			Graph g;

			if(service) {
				Node intent(stype, sname);

				gif *= intent;

				if (fallback) {
					Graph gi = Node() * intent;
					g = gi + gif;
				} else {
					g = gif;
				}
			} else {
				g = gif;
			}

			g.fill_sockaddr(&sa);
			if((rc =_append_addrinfo(pai, sa, socktype, protocol, cname)) != 0) {
				Xfreeifaddrs(ifaddr);
				return rc;
			}
		}
		Xfreeifaddrs(ifaddr);

		/*
		// user wants the name of the local system for binding to a suocket to be used in accept calls

		// NOTE: this code needs to be modified if we ever have the concept of INADDR_ANY or multiple interfaces or names for a host in XIA
		// at some point in the future local and loopback may require different code

		char dag[XIA_MAX_DAG_STR_SIZE], fid[XID_LEN];
		//char rv_ad[XID_LEN], rv_hid[XID_LEN], rv_sid[XID_LEN];
		char rv_dag_str[XIA_MAX_DAG_STR_SIZE];
		int rc;
		int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);

		if (sock < 0) {
			return EAI_SYSTEM;
		}

		rc = XreadLocalHostAddr(sock, dag, XIA_MAX_DAG_STR_SIZE, fid, XID_LEN);

		// Retrieve AD and HID from the system DAG
		// TODO: Revisit how we are building DAGs here, with the new
		//       multihoming paradigm that the base DAG should be
		//       provided by the router and hosts should not change it
		Graph g_localhost(dag);
		std::string ad = g_localhost.intent_AD_str();
		std::string hid = g_localhost.intent_HID_str();
		if(ad.size() == 0 || hid.size() == 0) {
			printf("ERROR: Unable to read localhost AD or HID\n");
			return EAI_SYSTEM;
		}

		Xclose(sock);
		if (rc < 0) {
			return EAI_SYSTEM;
		}

		// check the 4id and if it's correct, look at the last 8 chars to find the ip address
		// if they are 0.0.0.0 or 127.0.0.1, we can ignore the 4id as it's local only
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
		Node n_ad  = Node(ad.c_str());
		Node n_hid = Node(hid.c_str());
		Graph g = (n_src * n_ad * n_hid);
		if(socktype == SOCK_STREAM) {
			//rv_available = XreadRVServerAddr(rv_ad, XID_LEN, rv_hid, XID_LEN, rv_sid, XID_LEN);
			if(XreadRVServerAddr(rv_dag_str, XIA_MAX_DAG_STR_SIZE) == 0) {
				Graph g2 = (n_src * n_ad);
				Graph rvg(rv_dag_str);
				printf("Rendezvous DAG:\n%s\n", rvg.dag_string().c_str());
				Graph g3 = g2 * rvg * (n_hid);
				printf("Rendezvous Fallback:\n%s\n", g3.dag_string().c_str());
				//Node n_rvad = Node(rv_ad);
				//Node n_rvhid = Node(rv_hid);
				//Node n_rvsid = Node(rv_sid);
				//Graph g2 = (n_src * n_ad * n_rvad * n_rvhid * n_rvsid * n_hid);
				g = g + g3;
				printf("DAG after adding RV:\n%s\n", g.dag_string().c_str());
			}
		}

		if (have4id)
			g = g + (n_src * n_ip * n_ad * n_hid);

		if (fallback) {
			Graph gf = n_src;
			g = gf + g;
		}

		if (service)
			g = g * Node(stype, sname);

		g.fill_sockaddr(&sa);
		*/

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
		if((rc =_append_addrinfo(pai, sa, socktype, protocol, cname)) != 0) {
			return rc;
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
		if((rc =_append_addrinfo(pai, sa, socktype, protocol, cname)) != 0) {
			return rc;
		}
	}

	return 0;
}

/*!
** @brief Free all memory allocated by Xgetaddrinfo call
**
** @param ai Pointer to addrinfo list allocated by Xgetaddrinfo
**
*/
void Xfreeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *p;

	while(ai != NULL) {
		p = ai;
		ai = ai->ai_next;
		free(p->ai_addr);
		free(p);
	}
}
