//#define _GNU_SOURCE
/*
** Copyright 2015 Carnegie Mellon University
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

/*
** This library is loaded via LD_PRELOAD so that it comes before glibc.
** It captures all of the socket related functions so that they can be
** remapped to XIA function calls.
**
** The easiest way to use it is via the xia-core/bin/xwrap script
*/

/* FIXME:
**	- The __foo_chk functions currently call the foo function, not the checked
**	version. This is to avoid having to figure out the extra parameters. We
**	should probably call the right functions eventually although it doen't
**	affect the running code, just the checks for buffer sizes.
**
**	- DO THE RIGHT THING FOR SO_ERROR in the API
**	- Remove the FORCE_XIA calls and always default to XIA mode
**
**	- Add readv/writev support back in
**
*/

#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <limits.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <sched.h>
#include <algorithm>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include <algorithm>
#include "minIni.h"
#include <map>
#include <vector>

//#define PRINTBUFS

// defines **********************************************************
#define ADDR_MASK  "169.254.%d.%d"   // fake addresses created in this subnet
#define DEVICENAME "eth0"

#define ID_LEN (INET_ADDRSTRLEN + 10)	// size of an id string
#define SID_SIZE 45						// (40 byte XID + 4 bytes SID: + null terminator)
#define FORCE_XIA() (1)					// FIXME: get rid of this logic

#define SID_FILE "/etc/sidmap.conf"

// Logging Macros ***************************************************
#define TRACE()          {if (_log_trace)    fprintf(_log, "xwrap: %08x %s\n", getpid(), __FUNCTION__);}

#define MSG(...)         {if (_log_info)    {fprintf(_log, "xwrap: %08x %s ", getpid(), __FUNCTION__); fprintf(_log, __VA_ARGS__);}}
#define XFER_FLAGS(f)    {if (_log_info)     fprintf(_log, "xwrap: %08x %s flags:%s\n", getpid(), __FUNCTION__, xferFlags(f));}
#define AI_FLAGS(f)      {if (_log_info)     fprintf(_log, "xwrap: %08x %s flags:%s\n", getpid(), __FUNCTION__, aiFlags(f));}
#define FCNTL_FLAGS(f)   {if (_log_info)     fprintf(_log, "xwrap: %08x %s flags:%s\n", getpid(), __FUNCTION__, fcntlFlags(f));}
#define FCNTL_CMD(f)     {if (_log_info)     fprintf(_log, "xwrap: %08x %s command:%s\n", getpid(), __FUNCTION__, fcntlCmd(f));}
#define AF_VALUE(f)      {if (_log_info)     fprintf(_log, "xwrap: %08x %s family:%s\n", getpid(), __FUNCTION__, afValue(f));}
#define OPT_VALUE(f)     {if (_log_info)     fprintf(_log, "xwrap: %08x %s opt:%s\n", getpid(), __FUNCTION__, optValue(f));}
#define POLL_FLAGS(i, f) {if (_log_info)     fprintf(_log, "xwrap: %08x %s socket:%u %s\n", getpid(), __FUNCTION__, i, pollFlags(f));}

#define XIAIFY()         {if (_log_wrap)     fprintf(_log, "xwrap: %08x %s redirected to XIA\n", getpid(), __FUNCTION__);}
//#define NOXIA()        {if (_log_wrap)     fprintf(_log, "xwrap: %08x %s used normally\n", getpid(), __FUNCTION__);}
#define NOXIA()
#define SKIP()           {if (_log_wrap)     fprintf(_log, "xwrap: %08x %s not required/supported in XIA (no-op)\n", getpid(), __FUNCTION__);}

#define ALERT()          {if (_log_warning)  fprintf(_log, "xwrap: %08x ALERT!!!, %s is not implemented in XIA!\n", getpid(), __FUNCTION__);}
#define WARNING(...)     {if (_log_warning) {fprintf(_log, "xwrap: %08x %s ", getpid(), __FUNCTION__); fprintf(_log, __VA_ARGS__);}}

#ifdef DEBUG
#define DBG(...) {if (_log_warning) {fprintf(_log, "xwrap: %08x %s ", getpid(), __FUNCTION__); fprintf(_log, __VA_ARGS__);}}
#else
#define DBG(...)
#endif

// C style functions to avoid name mangling issue *******************
extern "C" {

int fcntl(int fd, int cmd, ...);

ssize_t __read_chk(int, void *, size_t, size_t);
ssize_t __recv_chk(int, void *, size_t, size_t, int);
ssize_t __recvfrom_chk (int, void *__restrict, size_t, size_t, int, __SOCKADDR_ARG, socklen_t *__restrict);
int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, __SIZE_TYPE__ __fdslen);

// Problem definitions. The compiler complains when I do these
// maybe make an external C only file for definitions with issues?
//int getnameinfo (const struct sockaddr *, socklen_t, char *, socklen_t, char *, socklen_t, unsigned int);
}

/*
** Define a typedef and declare a function pointer that uses it.
** The typedef is created so we can use it to cast the void * result from
** dlsym to the appropriate function pointer type in the GET_FCN macro.
**
** example:
**   DECLARE(int, socket, int domain, int type, int protocol); ==>
**
**   typedef int (*fcn_socket)(int domain, int type, int protocol);
**   fcn_socket __real_socket;
*/
#define DECLARE(r, f, ...)	\
	typedef r (*fcn_##f)(__VA_ARGS__);	\
	fcn_##f __real_##f

/*
** assign value of real function to a pointer so we can call it from
** the wrapper functions.
**
** example:
**   GET_FCN(socket) ==>
**
**   __real_socket = (fcn_socket)dlsym(RTLD_NEXT, "socket");
*/
#define GET_FCN(f)	__real_##f = (fcn_##f)dlsym(RTLD_NEXT, #f)

// Remapped functions ***********************************************
DECLARE(int, accept, int fd, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, bind, int fd, const struct sockaddr *addr, socklen_t len);
DECLARE(int, close, int fd);
DECLARE(int, connect, int fd, const struct sockaddr *addr, socklen_t len);
DECLARE(void, freeaddrinfo, struct addrinfo *ai);
DECLARE(void, freeifaddrs, struct ifaddrs *ifa);
DECLARE(const char *, gai_strerror, int ecode);
DECLARE(int, getaddrinfo, const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
DECLARE(int, getifaddrs, struct ifaddrs **ifap);
DECLARE(int, getpeername, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(int, getsockname, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(int, getsockopt, int fd, int level, int optname, void *optval, socklen_t *optlen);
DECLARE(int, fcntl, int fd, int cmd, ...);
DECLARE(pid_t, fork, void);
DECLARE(int, listen, int fd, int n);
DECLARE(int, poll, struct pollfd *fds, nfds_t nfds, int timeout);
DECLARE(ssize_t, read, int fd, void *buf, size_t count);
DECLARE(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt);
DECLARE(ssize_t, recv, int fd, void *buf, size_t n, int flags);
DECLARE(ssize_t, recvfrom, int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, select, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
DECLARE(ssize_t, send, int fd, const void *buf, size_t n, int flags);
DECLARE(ssize_t, sendto, int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len);
DECLARE(int, setsockopt, int fd, int level, int optname, const void *optval, socklen_t optlen);
DECLARE(int, shutdown, int fd, int how);
DECLARE(int, socket, int domain, int type, int protocol);
DECLARE(int, socketpair, int domain, int type, int protocol, int fds[2]);
DECLARE(ssize_t, write, int fd, const void *buf, size_t count);
DECLARE(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt);

// not ported to XIA, remapped for warning purposes *****************
DECLARE(struct hostent *,gethostbyaddr, const void *addr, socklen_t len, int type);
DECLARE(int, gethostbyaddr_r, const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(struct hostent *,gethostbyname, const char *name);
DECLARE(int, gethostbyname_r, const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, getnameinfo, const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags);
DECLARE(struct servent*, getservbyname, const char *name, const char *proto);
DECLARE(int, getservbyname_r, const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(struct servent*, getservbyport, int port, const char *proto);
DECLARE(int, getservbyport_r, int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(ssize_t, recvmsg, int fd, struct msghdr *msg, int flags);
DECLARE(ssize_t, sendmsg, int fd, const struct msghdr *msg, int flags);

DECLARE(int, execve, const char *filename, char *const argv[], char *const envp[]);

DECLARE(int, clone, int (*fn)(void *), void *child_stack,int flags, void *arg, ...);



// local "IP" address **********************************************
// When running in a local topology this means the client and server
// hosts end up with the same IP which can be a little confusing as
// they are running on different XIA hosts
struct ifaddrs default_ifa;
static char local_addr[ID_LEN];
static struct sockaddr local_sa;

typedef std::vector<std::string> address_t;

static address_t addresses;

// ID (IP-port) <=> DAG mapping tables *****************************
typedef std::map<std::string, std::string> id2dag_t;
typedef std::map<std::string, std::string> dag2id_t;

static id2dag_t id2dag;
static dag2id_t dag2id;

// HACK to allow services with fixed ports always get the same SID
// This will only work correctly for DATAGRAM sockets unless the SID
// is generated cryptographically and keys are distributed to
// all hosts that host the service
typedef std::map<unsigned short, std::string> port2sid_t;

static port2sid_t *port2sid;

// Negative name lookups are saved here ****************************
#define NEG_LOOKUP_LIFETIME	15	// duration in seconds of a negative lookup

typedef std::map<std::string, time_t> failed_t;
failed_t failedLookups;

// log output flags *************************************************
static int _log_trace = 0;
static int _log_warning = 0;
static int _log_info = 0;
static int _log_wrap = 0;
static int _log_dump = 0;
static FILE *_log = NULL;

/********************************************************************
**
** Helper functions
**
********************************************************************/
// call into the Xsockets API to see if the fd is associated with an Xsocket
#define isXsocket(s)	 (getSocketType(s) != -1)
#define shouldWrap(s)	 (isXsocket(s))


// get a unique port number
static unsigned short _NewPort()
{
	// it doesn't really matter in our case, but stay away from protected ports
	#define PROTECTED 1024

	// start with a random value
	static unsigned short port = PROTECTED + (rand() % (USHRT_MAX - PROTECTED));

	MSG("%d\n", port)

	if (++port < PROTECTED) {
		// we wrapped, set it back to the base
		port = PROTECTED;
	}
	return htons(port);
}

// load the list of static port to SID mappings
// the static mapping will only work if
// a) it is used with a DATAGRAM socket
// b) the SID was created crypographically and the associated
//  key is resident on the server binding to the SID
static void _LoadSIDs() 
{
	char conf[2048];
	char p[64];
	char s[64];
	unsigned short port;

	if (!XrootDir(conf, sizeof(conf))) {
		conf[0] = 0;
	}
	strncat(conf, SID_FILE, sizeof(conf));

	port2sid = new port2sid_t;

	for (int i = 0; ini_getkey(NULL, i, p, sizeof(p), conf); i++) {
		ini_gets(NULL, p, NULL, s, sizeof(s), conf);

		port = strtol(p, NULL, 10);
		std::string sid(s);
		if (errno != ERANGE) {
			MSG("Adding port<->SID mapping for %d : %s\n", port, sid.c_str());

			port = htons(port);
			port2sid->insert(std::pair<unsigned short, std::string>(port, sid));
		} else {
			WARNING("%s is not a valid port number\n", p);
		}
	}
}



/* Naming swiss army knife
** inputs:
**	addr: if non-NULL, use the address to create an ID and optionally map
**        to the specified DAG, otherwise create a new fake IP address
** port:  port to put into sin and for creating the ID string. Is specified
**        in network byte order
**  sax: if non-null, associate the DAG with sin in the internal mapping
**       tables
**
** outputs:
**  sin: sockaddr built from the IP address (or generated IP) and port
**
** returns:
**  currently always returns success
**
** a) If called as _GetIP(NULL, &sockaddr_in, ip, port)
**   fill in the sockaddr with the specified ip and port
** b) If called as _GetIP(NULL, &sockaddr_in, NULL, port)
**   create a fake IP address and fill in the sockaddr with it and port
** c) If called as _GetIP(&sax, &sockaddr_in, ip, port)
**   do the same as (a) and create an internal mapping between the DAG and sockaddr
** d) If called as _GetIP(&sax, &sockaddr_in, NULL, port)
**   do the same as (b) and create an internal mapping between the DAG and sockaddr
*/
static int _GetIP(const sockaddr_x *sax, struct sockaddr_in *sin, const char *addr, int port)
{
	// pick random IP address numbers 169.254.high.low
	static unsigned char low = (rand() % 253) + 1; // 1..254
	static unsigned char high = rand() % 254;      // 0..254

	char s[ID_LEN];
	char id[ID_LEN];

	// Make an IPv4 sockaddr
	if (!addr) {
		sprintf(s, ADDR_MASK, high, low);
		addr = s;

		// bump the ip address for next time
		low++;
		if (low == 255) {
			low = 1;
			high++;
			if (high == 255) {
				high = 0;
			}
		}
	}
	inet_pton(AF_INET, addr, &sin->sin_addr);

	sin->sin_family = AF_INET;
	sin->sin_port = port;

	// create an ID for the IPv4 sockaddr
	sprintf(id, "%s-%d", addr, ntohs(sin->sin_port));

	// if we have a DAG, create a mapping between the IPv4 sockaddr and the DAG
	if (sax) {
		Graph g(sax);

		std::string dag = g.dag_string();

		MSG("%s =>\n%s\n", id, dag.c_str());

		id2dag[id] = dag;
		dag2id[dag] = id;
	}

	return 0;
}



// Generate a random SID
static char *_NewSID(char *buf, unsigned len, unsigned short port)
{
	MSG("newsid port = %d\n", port);
	// note: buf must be at least 45 characters long
	// (longer if the XID type gets longer than 3 characters)
	if (len < SID_SIZE) {
		WARNING("buf is only %d chars long, it needs to be %d. Truncating...\n", len, SID_SIZE);
	}

	// lookup port, and if found use the static SID associated with it

	port2sid_t::iterator it = port2sid->find(port);
	if (it != port2sid->end()) {
		std::string sid = it->second;
		strncpy(buf, sid.c_str(), len);

	} else if (XmakeNewSID(buf, len)) {
		MSG("Unable to create a new SID\n");
		return NULL;
	}

	return buf;
}



// convert a IPv4 sockaddr into an id string in the form of A.B.C.D-port
static char *_IDstring(char *s, unsigned len, const struct sockaddr_in* sa)
{
	char ip[ID_LEN];

	inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);

	// make an id from the ip address and port
	snprintf(s, len, "%s-%u", ip, ntohs(sa->sin_port));
	return s;
}



// map from IP to XIA
static int _i2x(struct sockaddr_in *sin, sockaddr_x *sax)
{
	char s[ID_LEN];
	int rc = 0;

	id2dag_t::iterator it = id2dag.find(_IDstring(s, ID_LEN, sin));

	if (it != id2dag.end()) {

		std::string dag = it->second;

		MSG("Found: %s -> %s\n", s, dag.c_str());

		Graph g(dag);
		g.fill_sockaddr(sax);

	} else {
		MSG("No mapping for %s\n", s);
		rc = -1;
	}

	return rc;
}



// map from XIA to IP
static int _x2i(const sockaddr_x *sax, sockaddr_in *sin)
{
	char id[ID_LEN];
	Graph g(sax);
	int rc = 0;

	/*
	** NOTE: This depends on the created dag string
	** always looking the same across calls. It will
	** fail if the dag elements are reordered.
	*/
	dag2id_t::iterator it = dag2id.find(g.dag_string());

	if (it != dag2id.end()) {
		strcpy(id, it->second.c_str());
		MSG("Found: %s\n", id);

		// chop name into ip address and port
		char *p = strchr(id, '-');
		*p++ = 0;

		inet_pton(AF_INET, id, &sin->sin_addr);
		sin->sin_port = htons(atoi(p));
		sin->sin_family = AF_INET;

	} else {
		rc = -1;
		MSG("No mapping for %s\n", g.dag_string().c_str());
	}

	return rc;
}



// create a dag with sid for this sockaddr and register it with
// the xia name server. Also create a mapping to go from ip->xia and xia-ip
static int _Register(const struct sockaddr_in *sa)
{
	unsigned rc;
	char id[ID_LEN];
	char sid[SID_SIZE];
	struct addrinfo *ai;

	// create a DAG for this host in the form of "(4ID) AD HID SID"
	rc = Xgetaddrinfo(NULL, _NewSID(sid, sizeof(sid), sa->sin_port), NULL, &ai);

	if (rc != 0) {
		MSG("rc:%d err:%s\n", rc, strerror(rc));
	}

	// register it in the name server with the ip-port id
	XregisterName(_IDstring(id, ID_LEN, sa), (sockaddr_x*)ai->ai_addr);

	// put it into the mapping tables
	Graph g((sockaddr_x*)ai->ai_addr);
	std::string dag = g.dag_string();
	MSG("registered id:%s\ndag:%s\n", id, dag.c_str());

	id2dag[id] = dag;
	dag2id[dag] = id;

	return 1;
}



// check to see if we had a failed id lookup
// they are cached for 15 seconds then purged to allow
// for updates to the name server
static int _NegativeLookup(std::string id)
{
	int rc = 0;
	failed_t::iterator it = failedLookups.find(id);

	if (it != failedLookups.end()) {
		time_t t = it->second;

		if (time(NULL) - t >= NEG_LOOKUP_LIFETIME) {
			failedLookups.erase(it);
			MSG("clearing negative lookup entry for %s\n", id.c_str());

		} else {
			rc = -1;
		}
	}

	return rc;
}



// try to find the IPv4 ID/port ID associated with the given DAG
static int _ReverseLookup(const sockaddr_x *sax, struct sockaddr_in *sin)
{
	int rc = 0;
	char id[ID_LEN];
	socklen_t slen = sizeof(sockaddr_x);

	if (_x2i(sax, sin) < 0) {
		// we don't have a local mapping for this yet

		// See if it's in the nameserver
		if ( XgetNamebyDAG(id, ID_LEN, sax, &slen) >= 0) {
			// found on the name server
			MSG("reverse lookup = %s\n", id);

			// chop name into ip address and port
			char *p = strchr(id, '-');
			*p++ = 0;

			inet_pton(AF_INET, id, &sin->sin_addr);
			sin->sin_port = htons(atoi(p));
			sin->sin_family = AF_INET;

			*(p-1) = '-';

			// put it into the mapping tables
			Graph g(sax);
			std::string dag = g.dag_string();
			MSG("registered id:%s\ndag:%s\n", id, dag.c_str());

			id2dag[id] = dag;
			dag2id[dag] = id;

		} else {
			MSG("not found creating fake address\n");
			// create a fake IP address
			_GetIP(sax, sin, NULL, _NewPort());
		}
	}

	return rc;
}



// try to find a DAG associated with the IPv4 ip.port id
static int _Lookup(const struct sockaddr_in *addr, sockaddr_x *sax)
{
	int rc = 0;
	char id[ID_LEN];
	socklen_t len = sizeof(sockaddr_x);

	if (_i2x((struct sockaddr_in*)addr, sax) == 0) {
		// found locally
		return rc;
	}

	_IDstring(id, ID_LEN, addr);

	if (_NegativeLookup(id)) {
		return EAI_NONAME;
	}

	MSG("Looking up mapping for %s in the nameserver\n", id);

	if (XgetDAGbyName(id, sax, &len) < 0) {
		WARNING("Mapping server lookup failed for %s\n", id);

		failedLookups[id] = time(NULL);
		rc = EAI_NONAME;

	} else {
		Graph g(sax);
		std::string dag = g.dag_string();

		MSG("id:%s\ndag:%s\n", id, dag.c_str());
		// found it, add it to our local mapping tables
		id2dag[id] = dag;
		dag2id[dag] = id;
	}

	return rc;
}



static int _LookupStr(const char *id, unsigned short port, struct sockaddr *sa, sockaddr_x *sax)
{
	int rc = 0;
	struct sockaddr_in sin;

	bzero(&sin, sizeof(sin));
	if (inet_pton(AF_INET, id, &sin.sin_addr) > 0) {
		sin.sin_port = port;
		sin.sin_family = AF_INET;

		if ((rc = _Lookup(&sin, sax)) == 0) {
			memcpy(sa, &sin, sizeof(struct sockaddr));
		}
	} else {
		MSG("Hostnames are not supported at this time\n");
		rc = EAI_FAMILY;
	}

	return rc;
}



// see if the given IP address is local to our machine
static bool _isLocalAddr(const char* addr)
{
	MSG("local addr size = %zu\n", addresses.size());

	if (addr == NULL || strlen(addr) == 0)
		return true;

	address_t::iterator it = find(addresses.begin(), addresses.end(), addr);

	bool found = (it != addresses.end());

	MSG("%p %s: found:%d\n", &addresses, addr, found);
	return found;
}




// figure out the IP addresses that refer to this machine
// and pick a default one to use for our fake addressing
static int _GetLocalIPs()
{
	struct ifaddrs *ifa;
	struct ifaddrs *p;
	struct ifaddrs *def_ifa;
	char ip[ID_LEN];
	bool found = false;

	// we want INADDR_ANY
	addresses.push_back("0.0.0.0");

	__real_getifaddrs(&ifa);

	for (p = ifa; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr->sa_family == AF_INET) {

			struct sockaddr_in *sa = (struct sockaddr_in*)p->ifa_addr;

			inet_ntop(AF_INET, (void *)&sa->sin_addr, ip, ID_LEN);
			MSG("%s:%s\n", p->ifa_name, ip);

			addresses.push_back(ip);

			// prefer ETH0 as our default addrfess
			// but take others if it's not there
			if (!found) {
				def_ifa = p;
				strcpy(local_addr, ip);

				if (strcasecmp(p->ifa_name, "ETH0") == 0) {
					found = true;
				}
			}
		}
	}

	// this test had better be true!
	// save off the ifaddr we want to reply to apps with
	if (def_ifa) {
		p = def_ifa;
		memcpy(&default_ifa, p, sizeof(struct ifaddr));
		default_ifa.ifa_next = NULL;

		default_ifa.ifa_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
		default_ifa.ifa_netmask = (struct sockaddr *)malloc(sizeof(struct sockaddr));
		default_ifa.ifa_broadaddr = (struct sockaddr *)malloc(sizeof(struct sockaddr));

		memcpy(default_ifa.ifa_addr, p->ifa_addr, sizeof(struct sockaddr));
		memcpy(default_ifa.ifa_netmask, p->ifa_netmask, sizeof(struct sockaddr));
		memcpy(default_ifa.ifa_broadaddr, p->ifa_broadaddr, sizeof(struct sockaddr));
	}

	__real_freeifaddrs(ifa);

	// save default addr as a sockaddr_in too
	_GetIP(NULL, (struct sockaddr_in *)&local_sa, local_addr, 0);

	MSG("My Default IP = %s\n", local_addr);

	return 0;
}



static void selectDump(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, int in)
{
//	if (!_log_dump)
		return;

	MSG("%s\n", (in ? "PRE" : "POST"));
	for (int i = 0; i < nfds; i++) {
		if (readfds && FD_ISSET(i, readfds)) {
			MSG("%d = read\n", i);
		}
		if (writefds && FD_ISSET(i, writefds)) {
			MSG("%d = write\n", i);
		}
		if (exceptfds && FD_ISSET(i, exceptfds)) {
			MSG("%d = except\n", i);
		}
	}
}



// dump the contents of the pollfds
static void pollDump(struct pollfd *fds, nfds_t nfds, int in)
{
//	if (!_log_dump)
		return;

	MSG("%s\n", (in ? "PRE" : "POST"));
	for(nfds_t i = 0; i < nfds; i++) {
		if (in == 1 && fds[i].events != 0) {
			POLL_FLAGS(fds[i].fd, fds[i].events);

		} else if (fds[i].revents != 0) {
			POLL_FLAGS(fds[i].fd, fds[i].revents);
		}
	}
}



void bufferDump(int fd, int type, const void *buf, size_t len)
{
	if (!_log_dump)
		return;
	
	MSG("%d: %s...\n", fd, (type == 0 ? "READING" : "WRITING"));

	char *s = (char *)malloc(len + 10);
	memcpy(s, buf, len);
	s[len] = 0;
	MSG("buf: (%s)\n", s);
	free(s);
}



/********************************************************************
**
** Called at library load time for initialization
**
********************************************************************/
void __attribute__ ((constructor)) xwrap_init(void)
{
	if (_log_info || _log_warning || _log_wrap || _log_trace) {
		fprintf(_log, "loading XIA wrappers (created: %s)\n", __DATE__);
		fprintf(_log, "remapping all socket IO automatically into XIA\n");
	}

	// roll the dice
	srand(time(NULL));

	// enable logging
	if (getenv("XWRAP_TRACE") != NULL)
		_log_trace = 1;
	if (getenv("XWRAP_DUMP") != NULL)
		_log_dump = 1;
	if (getenv("XWRAP_VERBOSE") != NULL)
		_log_trace = _log_info = _log_wrap = _log_warning = 1;
	if (getenv("XWRAP_INFO") != NULL)
		_log_info = 1;
	if (getenv("XWRAP_WARNING") != NULL)
		_log_warning = 1;
	if (getenv("XWRAP_WRAP") != NULL)
		_log_wrap = 1;

	const char *lf = getenv("XWRAP_LOGFILE");
	if (lf)
		_log = fopen(lf, "w");
	if (!_log)
		_log = stderr;

	// find and save the real function pointers
	GET_FCN(accept);
	GET_FCN(bind);
	GET_FCN(close);
	GET_FCN(connect);
	GET_FCN(fcntl);
	GET_FCN(fork);
	GET_FCN(freeaddrinfo);
	GET_FCN(freeifaddrs);
	GET_FCN(gai_strerror);
	GET_FCN(getaddrinfo);
	GET_FCN(getifaddrs);
	GET_FCN(getpeername);
	GET_FCN(getsockname);
	GET_FCN(getsockopt);
	GET_FCN(listen);
	GET_FCN(poll);
	GET_FCN(read);
	GET_FCN(readv);
	GET_FCN(recv);
	GET_FCN(recvfrom);
	GET_FCN(select);
	GET_FCN(send);
	GET_FCN(sendto);
	GET_FCN(setsockopt);
	GET_FCN(shutdown);
	GET_FCN(socket);
	GET_FCN(socketpair);
	GET_FCN(write);
	GET_FCN(writev);	

	// do the same for the informational functions
	GET_FCN(gethostbyaddr);
	GET_FCN(gethostbyaddr_r);
	GET_FCN(gethostbyname);
	GET_FCN(gethostbyname_r);
	GET_FCN(getnameinfo);
	GET_FCN(getservbyname);
	GET_FCN(getservbyname_r);
	GET_FCN(getservbyport);
	GET_FCN(getservbyport_r);
	GET_FCN(recvmsg);
	GET_FCN(sendmsg);

	GET_FCN(execve);
	GET_FCN(clone);

	_GetLocalIPs();
	_LoadSIDs();
}


/********************************************************************
**
** FUNCTION REMAPPINGS START HERE
**
********************************************************************/
int accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
	int rc;
	sockaddr_x sax;
	struct sockaddr *ipaddr;
	socklen_t xlen;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		xlen = sizeof(sax);
		rc = Xaccept(fd, addr, &xlen);

		if (rc >= 0) {
			errno = 0;
		}

		MSG("RC = %d errno = %d ===============================================\n", rc, errno);

		if (FORCE_XIA()) {
			// create a new fake IP address/port  to map to
			_GetIP(&sax, (struct sockaddr_in*)ipaddr, NULL, _NewPort());

			// convert the sockaddr_x to a sockaddr
			_x2i(&sax, (struct sockaddr_in*)ipaddr);
		}

	} else {
		NOXIA();
		rc = __real_accept(fd, addr, addr_len);
	}
	return rc;
}



int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	int rc;
	sockaddr_x sax;
	sockaddr_in sin;

	TRACE();
	if (shouldWrap(fd) ) {
		XIAIFY();

		// swap in our local ip if the caller specified INADDR_ANY
		if (memcmp("\0\0\0\0", &((sockaddr_in*)addr)->sin_addr, 4) != 0) {
			MSG(" using supplied address\n");
			memcpy(&sin, addr, sizeof(sockaddr_in));
		} else {
			MSG(" using default address\n");
			memcpy(&sin, &local_sa, sizeof(sockaddr_in));
			sin.sin_port = ((struct sockaddr_in*)addr)->sin_port;
		}

		if (FORCE_XIA()) {
			// create a mapping from IP/port to a dag and register it
			_Register(&sin);

			char id[ID_LEN];
			_IDstring(id, ID_LEN, &sin);
			MSG("id:%s\n", id);

			// convert the sockaddr to a sockaddr_x
			_i2x(&sin, &sax);
		}

		rc = Xbind(fd, (struct sockaddr *)&sax, len);

	} else {
		NOXIA();
		rc = __real_bind(fd, addr, len);
	}

	return rc;
}



int close(int fd)
{
	sockaddr_x addr;
	socklen_t len = sizeof(addr);
	int rc;

//	TRACE();
	if (shouldWrap(fd)) {
		XIAIFY();
		MSG("closing %d\n", fd);

		// clean up entries in the dag2id and id2dag maps
		if (Xgetsockname(fd, (struct sockaddr *)&addr, &len) == 0) {

			Graph g(&addr);
			std::string dag = g.dag_string();
			std::string id = dag2id[dag];

			dag2id.erase(dag);
			id2dag.erase(id);
		}

		// kill it
		rc = Xclose(fd);

	} else {
		NOXIA();
		rc = __real_close(fd);
	}
	return rc;
}



int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	int rc;
	sockaddr_x sax;
	socklen_t slen = sizeof(sax);

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		if (_Lookup((sockaddr_in *)addr, &sax) != 0) {
			char id[ID_LEN];

			_IDstring(id, ID_LEN, (sockaddr_in*)addr);
			WARNING("Unable to lookup %s\n", id);
			errno = ENETUNREACH;
			return -1;
		}

		rc= Xconnect(fd, (const sockaddr*)&sax, slen);

	} else {
		NOXIA();
		rc = __real_connect(fd, addr, len);
	}
	return rc;
}



extern "C" int fcntl (int fd, int cmd, ...)
{
	int rc;
	va_list args;
	fcn_fcntl f;

//	TRACE();

	bool xia = isXsocket(fd);

	if (xia) {
		XIAIFY();
		f = Xfcntl;

		MSG("socket:%d X:%d\n", fd, isXsocket(fd));
		FCNTL_CMD(cmd);

	} else {
		NOXIA();
		f = __real_fcntl;
	}

	va_start(args, cmd);

	switch(cmd) {
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
		case F_SETFD:
		case F_SETFL:
		case F_SETOWN:
		case F_SETOWN_EX:
		case F_SETSIG:
		case F_SETLEASE:
		case F_NOTIFY:
		case F_SETPIPE_SZ:
		{
			int x = va_arg(args, int);

			if ((cmd == F_SETFL) && xia) {
				FCNTL_FLAGS(x);
			}

			rc = (f)(fd, cmd, x);
			break;
		}

		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			rc = (f)(fd, cmd, va_arg(args, struct flock *));
			break;

		case F_GETFD:
		case F_GETFL:
		case F_GETOWN:
		case F_GETOWN_EX:
		case F_GETSIG:
		case F_GETLEASE:
		case F_GETPIPE_SZ:
			rc = (f)(fd, cmd);
			break;

		default:
			WARNING("Unknown command value (%08x)\n", cmd);
			errno = EINVAL;
			rc = -1;
			break;
	}

	va_end(args);

	if (rc < 0)
		MSG("rc = %d: %s\n", rc, strerror(errno));

	return rc;
}

pid_t fork(void)
{
	TRACE();

	return Xfork();
}



void freeaddrinfo (struct addrinfo *ai)
{
	TRACE();
	// there currently isn't any new XIA functionality for this call
	return __real_freeaddrinfo(ai);
}



void freeifaddrs(struct ifaddrs *ifa)
{
	TRACE();

	// right now we don't need to do anything special, so let the system do it
	NOXIA();
	__real_freeifaddrs(ifa);
}



const char *gai_strerror (int ecode)
{
	TRACE();
	// there currently isn't any new XIA functionality for this call
	return __real_gai_strerror(ecode);
}



int getaddrinfo (const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **pai)
{
	int socktype = 0;
	int protocol = 0;
	int family = 0;
	int flags = 0;
	int rc = 0;
	unsigned short port = 0;
	bool localonly = false;

	/*
	** NOTES:
	**	The AI_ALL, AI_V4MAPPED, AI_ADDRCONFIG, AI_CANONIDN, & AI_IDN* flags are ignored
	*/

	TRACE();

	// UGLY HACK - where did the data in this table go????
	if (addresses.size() == 0) {
		MSG("addresses table is empty! reloading...\n");
		_GetLocalIPs();
	}

	*pai = NULL;

	if (name == NULL && service == NULL) {
		return EAI_NONAME;
	}

	if (hints) {
		// let's see if we can steal it
		if (hints->ai_family == AF_UNSPEC || hints->ai_family == AF_INET) {
			family = AF_INET;
		} else if (hints->ai_family == AF_XIA) {
			family = AF_XIA;
		} else {
			family = hints->ai_family;
		}

		socktype = hints->ai_socktype;
		protocol = hints->ai_protocol;

		if (hints->ai_flags != 0) {

			AI_FLAGS(hints->ai_flags);
			flags = hints->ai_flags;

			if (flags & AI_PASSIVE) {
				localonly = true;
			}
		}
	} else {
		// no family specified, so we're taking it and
		// hopefully will be correct
		family = AF_INET;
	}

	MSG("name:%s svc:%s st:%d pro:%d fam:%d\n", name, service, socktype, protocol, family);

	if (family == AF_INET) {
		// we're going to hijack the IPv4 name lookup

		char s[ID_LEN];
		struct sockaddr *sa;
		sockaddr_x sax;
//		socklen_t len = sizeof(sax);

		if (service) {
			if (flags & AI_NUMERICSERV) {
				localonly = true;
			}
			port = htons(strtol(service, NULL, 10));
			if (errno == EINVAL) {
				// service was not an integer

				if (flags & AI_NUMERICSERV) {
					// FIXME: is this correct, need more research on this flag
					return EAI_SERVICE;
				}

				struct servent *serv = getservbyname(service, NULL);
				if (serv) {
					port = serv->s_port;
				}
				endservent();
			}
		}

		if (localonly || _isLocalAddr(name)) {
			localonly = true;
			MSG("getting address for local machine\n");
		} else {
			MSG("need to do name resolution\n");
		}

		sa = (struct sockaddr *)calloc(sizeof(struct sockaddr), 1);

		if (localonly) {
			// caller wants a sockaddr for itself
			// use our default ip address, regardless of what they gave us

			memcpy(sa, &local_sa, sizeof(struct sockaddr));
			((sockaddr_in *)sa)->sin_port = port;
			MSG("returning address:%s port:%u\n", local_addr, htons(port));
			rc = 0;

		} else if (flags & AI_NUMERICHOST) {
			// just make a sockaddr with the passed info
			_GetIP(NULL, (sockaddr_in *)sa, name, port);
			rc = 0;

		} else {
			sprintf(s, "%s-%d", name, ntohs(port));

			if (_LookupStr(name, port, sa, &sax) < 0) {

				MSG("name lookup failed for %s\n", s);

				failedLookups[s] = time(NULL);
				rc =  EAI_NONAME;
				free(sa);
				goto done;
			}

			Graph g(&sax);
			std::string dag = g.dag_string();

			MSG("found id\n%s\n", dag.c_str());
		}

		struct addrinfo *ai = (struct addrinfo *)calloc(sizeof(struct addrinfo), 1);
		// fill in the blanks

		ai->ai_family    = family;
		ai->ai_socktype  = socktype;
		ai->ai_protocol  = protocol;
		ai->ai_flags     = flags;
		ai->ai_addrlen   = sizeof(struct sockaddr);
		ai->ai_next      = NULL;
		ai->ai_addr 	 = sa;

		if (flags & AI_CANONNAME) {
			char *cname =(char *)malloc(ID_LEN);
			char id[ID_LEN];

			inet_ntop(AF_INET, &((struct sockaddr_in*)sa)->sin_addr, id, ID_LEN);
			sprintf(cname, "%s-%d", id, ntohs(((struct sockaddr_in*)sa)->sin_port));
			ai->ai_canonname = cname;

		} else {
			ai->ai_canonname = NULL;
		}

		*pai = ai;
		rc = 0;

	} else if (family == AF_XIA) {
		rc= Xgetaddrinfo(name, service, hints, pai);

	} else {
		// XIA lookup failed, fall back to
		NOXIA();
		rc = __real_getaddrinfo(name, service, hints, pai);
	}
done:
	return rc;
}


// FIXME: do I still need to fake this if I'm using the real IP address for things?
int getifaddrs(struct ifaddrs **ifap)
{
	int rc = 0;
	struct ifaddrs *ifa = (struct ifaddrs*)calloc(1, sizeof(struct ifaddrs));

	TRACE();

	ifa->ifa_next      = NULL;
	ifa->ifa_name      = strdup(DEVICENAME);
	ifa->ifa_addr      = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
	ifa->ifa_netmask   = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
	ifa->ifa_broadaddr = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
	ifa->ifa_flags     = IFF_UP | IFF_RUNNING | IFF_BROADCAST;
	ifa->ifa_data      = NULL;

	memcpy(ifa->ifa_addr, default_ifa.ifa_addr, sizeof(struct sockaddr));
	memcpy(ifa->ifa_netmask, default_ifa.ifa_netmask, sizeof(struct sockaddr));
	memcpy(ifa->ifa_broadaddr, default_ifa.ifa_broadaddr, sizeof(struct sockaddr));

	// we aren't going to use the real version in the wrapper at all
	//rc = __real_getifaddrs(ifap);

	*ifap = ifa;
	return rc;
}



int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;
	sockaddr_x sax;
	socklen_t slen = sizeof(sax);

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		rc = Xgetpeername(fd, (struct sockaddr*)&sax, &slen);

		if (_x2i(&sax, (struct sockaddr_in*)addr) < 0) {
			_GetIP(&sax, (sockaddr_in*)addr, NULL, _NewPort());
		}

	} else {
		NOXIA();
		return __real_getpeername(fd, addr, len);
	}
	return rc;
}



int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;
	sockaddr_x sax;
	socklen_t slen = sizeof(sax);

	TRACE();
	if (shouldWrap(fd)) {
		XIAIFY();

		MSG("size:%lu len:%u\n", sizeof(struct sockaddr), *len);

		rc = Xgetsockname(fd, (struct sockaddr*)&sax, &slen);

		if (_x2i(&sax, (struct sockaddr_in*)addr) < 0) {
			_GetIP(&sax, (sockaddr_in*)addr, NULL, _NewPort());
		}

	} else {
		NOXIA();
		rc = __real_getsockname(fd, addr, len);
	}
	return rc;
}



int getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
	int rc;
	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		OPT_VALUE(optname);
		rc =  Xgetsockopt(fd, optname, optval, optlen);

		if (rc == 0) {
			if (optname == SO_DOMAIN && *(int*)optval == AF_XIA) {
				// keep hiding xia from the application
				*(int*)optval = AF_INET;
			}
		} else {

			// TODO: add code here to return success for options we can safely ignore
		}

	} else {
		NOXIA();
		rc = __real_getsockopt(fd, level, optname, optval, optlen);
	}
	return rc;
}



int listen(int fd, int n)
{
	int rc = 0;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		rc = Xlisten(fd, n);

	} else {
		NOXIA();
		rc = __real_listen(fd, n);
	}

	return rc;
}



int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int rc;
//	TRACE();

	// Let Xpoll do all the work of figuring out what fds we are handling
	pollDump(fds, nfds, 1);
	rc = Xpoll(fds, nfds, timeout);

	if (rc > 0) {
		pollDump(fds, nfds, 0);
	} else if (rc == 0) {
//		MSG("timeout\n");
	}

	return rc;
}



ssize_t read(int fd, void *buf, size_t n)
{
	size_t rc;

//	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		rc = Xrecv(fd, buf, n, 0);
		bufferDump(fd, 0, buf, rc);

	} else {
		NOXIA();
		rc = __real_read(fd, buf, n);
	}
	return rc;
}




ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	int rc;

//	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		// FIXME: handle EINVAL processing (probably will never occur so low priority)

		// make a buffer as large as available space in the iovec
		size_t size = _iovSize(iov, iovcnt);
		char *buf = (char *)malloc(size);

		rc = Xrecv(fd, buf, size, 0);

		if (rc >= 0) {
			rc = _iovUnpack(iov, iovcnt, buf, rc);
		}
		free(buf);

	} else {
		rc = __real_readv(fd, iov, iovcnt);
	}
	return rc;
}




ssize_t recv(int fd, void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		XFER_FLAGS(flags);
		rc = Xrecv(fd, buf, n, flags);
		bufferDump(fd, 0, buf, rc);

	} else {
		NOXIA();
		rc = __real_recv(fd, buf, n, flags);
	}

	return rc;
}



ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	int rc;
	sockaddr_x sax;
	socklen_t slen = sizeof(sax);
	sockaddr_x *addrx = NULL;
	bool do_address = false;

	TRACE();

	if(isXsocket(fd)) {
		XIAIFY();
		XFER_FLAGS(flags);

		if (addr != NULL) {
			do_address = true;
			addrx = &sax;
		}

		rc = Xrecvfrom(fd, buf, n, flags, (struct sockaddr*)addrx, &slen);

		MSG("fd:%d size:%zu returned:%d\n", fd, n, rc);

		if (rc >= 0 && do_address) {
			// convert the sockaddr_x to a sockaddr
			_ReverseLookup(addrx, (struct sockaddr_in*)addr);
		}

	} else {
		NOXIA();
		rc = __real_recvfrom(fd, buf, n, flags, addr, addr_len);
	}

	return rc;
}



ssize_t recvmsg(int fd, struct msghdr *msg, int flags)
{
	int rc;
	bool do_address = false;
	struct msghdr mh;
	sockaddr_x sax;

//	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		memcpy(&mh, msg, sizeof(struct msghdr));

		if (mh.msg_name != NULL) {
			do_address = true;
			mh.msg_name = &sax;
			mh.msg_namelen = sizeof(sax);
		}

		rc = Xrecvmsg(fd, &mh, flags);

		if (rc >= 0 && do_address) {
			// convert the sockaddr_x to a sockaddr
			_ReverseLookup(&sax, (struct sockaddr_in*)msg->msg_name);
		}

	} else {
		NOXIA();
		rc = __real_recvmsg(fd, msg, flags);
	}

	return rc;
}



int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	int rc;

	TRACE();

	selectDump(nfds, readfds, writefds, exceptfds, 0);

	// Let Xselect do all the work of figuring out what fds we are handling
	rc = Xselect(nfds, readfds, writefds, exceptfds, timeout);

	if (rc > 0) {
		// we found at least one event
		selectDump(nfds, readfds, writefds, exceptfds, 1);
	}

	return rc;
}



ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		XFER_FLAGS(flags);
		MSG("sending:%zu\n", n);
		bufferDump(fd, 1, buf, n);

		rc = Xsend(fd, buf, n, flags);

	} else {
		NOXIA();
		rc = __real_send(fd, buf, n, flags);
	}
	return rc;
}



ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
	int rc;
	struct msghdr mh;
	sockaddr_x sax;

//	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		if (_Lookup((sockaddr_in *)msg->msg_name, &sax) != 0) {
			char id[ID_LEN];

			_IDstring(id, ID_LEN, (sockaddr_in*)msg->msg_name);
			WARNING("Unable to lookup %s\n", id);
			errno = EINVAL;	// FIXME: is there a better return we can use here?
			return -1;
		}

		// make an XIA specific msg header with translated address
		memcpy(&mh, msg, sizeof(struct msghdr));
		mh.msg_name = (struct sockaddr*)&sax;
		mh.msg_namelen = sizeof(sax);

		rc = Xsendmsg(fd, &mh, flags);

	} else {
		NOXIA();
		rc = __real_sendmsg(fd, msg, flags);
	}

	return rc;
}



ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
{
	int rc;
	sockaddr_x sax;

	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		XFER_FLAGS(flags);

		if (FORCE_XIA()) {

			if (_Lookup((sockaddr_in *)addr, &sax) != 0) {
				char id[ID_LEN];

				_IDstring(id, ID_LEN, (sockaddr_in*)addr);
				WARNING("Unable to lookup %s\n", id);
				errno = EINVAL;	// FIXME: is there a better return we can use here?
				return -1;
			}

			addr_len = sizeof(sax);
			addr = (struct sockaddr*)&sax;
		}
		MSG("sending:%zu\n", n);

		rc = Xsendto(fd, buf, n, flags, addr, addr_len);

	} else {
		NOXIA();
		rc = __real_sendto(fd, buf, n, flags, addr, addr_len);
	}

	return rc;
}



int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int rc;
	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		OPT_VALUE(optname);

		rc =  Xsetsockopt(fd, optname, optval, optlen);

		if (rc < 0) {
			switch(optname) {
				case SO_DEBUG:
				case SO_ERROR:
				case SO_RCVTIMEO:
					// handled by Xsetsockopt
					break;

				case SO_DONTROUTE:
				case SO_REUSEADDR:
				case SO_SNDTIMEO:
				case SO_SNDBUF:
				case SO_RCVBUF:
				case SO_LINGER:
				case SO_KEEPALIVE:
				case SO_REUSEPORT:
					MSG("Unhandled option returning success %s\n", optValue(optname));
					rc = 0;
					break;

				case SO_TYPE:
				case SO_BROADCAST:
				case SO_SNDBUFFORCE:
				case SO_RCVBUFFORCE:
				case SO_OOBINLINE:
				case SO_NO_CHECK:
				case SO_PRIORITY:
				case SO_BSDCOMPAT:
				case SO_PASSCRED:
				case SO_PEERCRED:
				case SO_RCVLOWAT:
				case SO_SNDLOWAT:
					MSG("Unhandled option returning error %s\n", optValue(optname));
					break;

				default:
					MSG("Unknown socket option command %s\n", optValue(optname));
					break;
			}
			// TODO: add code here to return success for options we can safely ignore
		}

	} else {
		NOXIA();
		rc = __real_setsockopt(fd, level, optname, optval, optlen);
	}

	return rc;
}



int shutdown(int fd, int how)
{
	int rc;
	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR) {
			errno = EINVAL;
			rc = -1;

		} else if (getConnState(fd) != CONNECTED) {
			errno = ENOTCONN; 
			rc = -1;
		
		} else {
			// we don't have anything like shutdown in XIA, so lie and say we did something
			rc = 0;
		}		
	
	} else {
		NOXIA();
		rc = __real_shutdown(fd, how);
	}

	return rc;
}



int socket(int domain, int type, int protocol)
{
	int fd;
	TRACE();

	if (domain == AF_XIA || domain == AF_INET) {
		XIAIFY();
		AF_VALUE(domain);

		// is it unspecified, or TCP, or UDP?
		if (protocol != 0 && protocol != 6 && protocol != 17) {
			MSG("unknown protocol (%d) returning error", protocol);
			errno = EINVAL;
			return -1;
		}

		fd = Xsocket(AF_XIA, type, 0);

	} else {
		NOXIA();
		fd = __real_socket(domain, type, protocol);
	}
	return fd;
}


// FIXME - do I really need this. Seems like it is only useful for
// unix_domain sockets and sendmsg/recvmsg
int socketpair(int domain, int type, int protocol, int fds[2])
{
	TRACE();
	if (domain == AF_XIA || domain == AF_INET) {
		XIAIFY();
		fds[0] = socket(AF_XIA, type, protocol);
		fds[1] = socket(AF_XIA, type, protocol);

		if (fds[0] >= 0 && fds[1] >= 0) {
			return 0;
		} else {
			int e = errno;

			Xclose(fds[0]);
			Xclose(fds[1]);
			errno = e;
			return -1;
		}

	} else {
		NOXIA();
		return __real_socketpair(domain, type, protocol, fds);
	}
}



ssize_t write(int fd, const void *buf, size_t n)
{
	size_t rc;

//	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		bufferDump(fd, 1, buf, n);
		rc = Xsend(fd, buf, n, 0);

	} else {
		NOXIA();
		rc = __real_write(fd, buf, n);
	}
	return rc;
}




ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	int rc;

//	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		// writev is atomic, so put everything into a buffer instead of
		// writing out each individual iovec
		char *buf = NULL;
		size_t size = _iovPack(iov, iovcnt, &buf);

		MSG("sending:%zu\n", size);

		rc = send(fd, buf, size, 0);
		free(buf);

	} else {
		rc = __real_writev(fd, iov, iovcnt);
	}
	return rc;
}



/********************************************************************
** SHADOW FUNCTION MAPPINGS
********************************************************************/
extern "C" int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, __SIZE_TYPE__ __fdslen)
{
	int rc;
	UNUSED(__fdslen);
	TRACE();

	// Let Xpoll do all the work of figuring out what fds we are handling
	pollDump(fds, nfds, 1);
	rc = Xpoll(fds, nfds, timeout);

	if (rc > 0) {
		pollDump(fds, nfds, 0);
	} else if (rc == 0) {
		MSG("timeout\n");
	}

	return rc;
}



extern "C" ssize_t __read_chk (int __fd, void *__buf, size_t __nbytes, size_t __buflen)
{
	UNUSED(__buflen);
	TRACE();
	return read(__fd, __buf, __nbytes);
}



extern "C" ssize_t __recv_chk(int __fd, void *__buf, size_t __n, size_t __buflen, int __flags)
{
	UNUSED(__buflen);
	TRACE();
	return recv(__fd, __buf, __n, __flags);
}



extern "C" ssize_t __recvfrom_chk (int __fd, void *__restrict __buf, size_t __n, size_t __buflen, int __flags,
	__SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)
{
	UNUSED(__buflen);
	TRACE();
	return recvfrom(__fd, __buf, __n, __flags, __addr, __addr_len);
}

int execve(const char *filename, char *const argv[], char *const envp[])
{
	int rc;

	TRACE();

	rc = __real_execve(filename, argv, envp);
	return rc;
}

/********************************************************************
** INFO ONLY FUNCTION MAPPINGS
********************************************************************/
struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA
	return __real_gethostbyaddr(addr, len, type);
}



int gethostbyaddr_r (const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA
	return __real_gethostbyaddr_r(addr, len, type, result_buf, buf, buflen, result, h_errnop);
}



struct hostent *gethostbyname (const char *name)
{
	TRACE();
	ALERT();
	MSG("name=%s\n", name);

	// There's no state to work with so we have to assume
	// everything is for XIA

	// FIXME: add code here to map between IPv4 and XIA
	return __real_gethostbyname(name);
}



int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	MSG("name=%s\n", name);

	// FIXME: add code here to map between IPv4 and XIA
	return __real_gethostbyname_r(name, result_buf, buf, buflen, result, h_errnop);
}



int getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags)
{
	TRACE();
	ALERT();
	return __real_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}



struct servent *getservbyname (const char *name, const char *proto)
{
	TRACE();
	ALERT();
	return __real_getservbyname(name, proto);
}



int getservbyname_r (const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	TRACE();
	ALERT();
	return __real_getservbyname_r(name, proto, result_buf, buf, buflen, result);
}



struct servent *getservbyport (int port, const char *proto)
{
	TRACE();
	ALERT();
	return __real_getservbyport(port, proto);
}



int getservbyport_r (int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	TRACE();
	ALERT();
	return __real_getservbyport_r(port, proto, result_buf, buf, buflen, result);
}

int clone(int (*fn)(void *), void *child_stack,
                 int flags, void *arg, ...
                 /* pid_t *ptid, struct user_desc *tls, pid_t *ctid */ )
{
	TRACE();
	ALERT();
	return __real_clone(fn, child_stack, flags, arg);
}
