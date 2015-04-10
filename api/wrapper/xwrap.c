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
**	The __foo_chk functions currently call the foo function, not the checked
**	version. This is to avoid having to figure out the extra parameters. We
**	should probably call the right functions eventually although it doen't
**	affect the running code, just the checks for buffer sizes.
**
**	Remove the FORCE_XIA calls and always default to XIA mode 
*/
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
#include <map>

// defines **********************************************************
#define ADDR_MASK "169.254.%d.%d"		// fake addresses created in this subnet
#define ID_LEN (INET_ADDRSTRLEN + 10)	// size of an id string
#define SID_SIZE 45						// (40 byte XID + 4 bytes SID: + null terminator)
#define FORCE_XIA() (1)					// FIXME: get rid of this logic

// Logging Macros ***************************************************
#define TRACE()		 {if (_log_trace)    fprintf(_log, "xwrap: %s\r\n", __FUNCTION__);}

#define MSG(...)	 {if (_log_info)    {fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}}

#define XIAIFY()	 {if (_log_wrap)     fprintf(_log, "xwrap: %s redirected to XIA\r\n", __FUNCTION__);}
#define NOXIA()		 {if (_log_wrap)     fprintf(_log, "xwrap: %s used normally\r\n", __FUNCTION__);}
#define SKIP()		 {if (_log_wrap)     fprintf(_log, "xwrap: %s not required/supported in XIA (no-op)\r\n", __FUNCTION__);}

#define ALERT()		 {if (_log_warning)  fprintf(_log, "xwrap: ALERT!!!, %s is not implemented in XIA!\r\n", __FUNCTION__);}
#define WARNING(...) {if (_log_warning) {fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}}

#ifdef DEBUG

#else
#define DBG(...) {if (_log_warning) {fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}}
#define DBG(...)
#endif

// C style functions to avoid name mangling issue *******************
extern "C" {

int fcntl(int fd, int cmd, ...);
int ioctl(int d, int request, ...);

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
DECLARE(const char *, gai_strerror, int ecode);
DECLARE(int, getaddrinfo, const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
DECLARE(int, getpeername, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(int, getsockname, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(int, getsockopt, int fd, int level, int optname, void *optval, socklen_t *optlen);
DECLARE(int, fcntl, int fd, int cmd, ...);
DECLARE(void, freeaddrinfo, struct addrinfo *ai);
DECLARE(int, ioctl, int d, int request, ...);
DECLARE(int, listen, int fd, int n);
DECLARE(int, poll, struct pollfd *fds, nfds_t nfds, int timeout);
DECLARE(ssize_t, read, int fd, void *buf, size_t count);
DECLARE(ssize_t, recv, int fd, void *buf, size_t n, int flags);
DECLARE(ssize_t, recvfrom, int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, select, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
DECLARE(ssize_t, send, int fd, const void *buf, size_t n, int flags);
DECLARE(ssize_t, sendto, int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len);
DECLARE(int, setsockopt, int fd, int level, int optname, const void *optval, socklen_t optlen);
DECLARE(int, socket, int domain, int type, int protocol);
DECLARE(int, socketpair, int domain, int type, int protocol, int fds[2]);
DECLARE(ssize_t, write, int fd, const void *buf, size_t count);

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
DECLARE(ssize_t, recvmsg, int fd, struct msghdr *message, int flags);
DECLARE(ssize_t, sendmsg, int fd, const struct msghdr *message, int flags);


// ID (IP-port) <=> DAG mapping tables *****************************
typedef std::map<std::string, std::string> id2dag_t;
typedef std::map<std::string, std::string> dag2id_t;

id2dag_t id2dag;
dag2id_t dag2id;

// Negative name lookups are saved here ****************************
#define NEG_LOOKUP_LIFETIME	15	// duration in seconds of a negative lookup

typedef std::map<std::string, time_t> failed_t;
failed_t failedLookups;

// log output flags *************************************************
static int _log_trace = 0;
static int _log_warning = 0;
static int _log_info = 0;
static int _log_wrap = 0;
static FILE *_log = NULL;

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

	// cause the Xsocket API to load the pointers to the real socket functions
	// for it's own internal use 
	xapi_load_func_ptrs();

	// roll the dice
	srand(time(NULL));

	// enable logging
	if (getenv("XWRAP_TRACE") != NULL)
		_log_trace = 1;
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
	GET_FCN(freeaddrinfo);
	GET_FCN(gai_strerror);
	GET_FCN(getaddrinfo);
	GET_FCN(getpeername);
	GET_FCN(getsockname);
	GET_FCN(getsockopt);
	GET_FCN(ioctl);
	GET_FCN(listen);
	GET_FCN(poll);
	GET_FCN(read);
	GET_FCN(recv);
	GET_FCN(recvfrom);
	GET_FCN(select);
	GET_FCN(send);
	GET_FCN(sendto);
	GET_FCN(setsockopt);
	GET_FCN(socket);
	GET_FCN(socketpair);
	GET_FCN(write);

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
}

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
	return port;
}

// Create a new fake IP address to associate with the given DAG
static int _GetIP(sockaddr_x *sax, struct sockaddr_in *sin, const char *addr, int port)
{
	// pick random IP address numbers 169.254.high.low
	static unsigned char low = (rand() % 253) + 1;
	static unsigned char high = rand() % 254;

	char s[ID_LEN];
	char id[ID_LEN];

	// Make an IPv4 sockaddr
	if (port == 0)
		port = _NewPort();

	if (!addr) { 
		sprintf(s, ADDR_MASK, high, low);
		addr = s;
#if 0
		// FIXME: do we want to increment this each time or just set it once for each app?
		// bump the ip address for next time
		low++;
		if (low == 254) {
			low = 1;
			high++;
			if (high == 255) {
				high = 0;
			}
		}
#endif
	}
	inet_aton(addr, &sin->sin_addr);

	sin->sin_family = AF_INET;
	sin->sin_port = port;

	// create an ID for the IPv4 sockaddr
	sprintf(id, "%s-%d", addr, ntohs(sin->sin_port));

	// if we have a DAG, create a mapping between the IPv4 sockaddr and the DAG
	if (sax) {
		Graph g(sax);

		std::string dag = g.dag_string();

//		MSG("%s =>\n%s\n", id, dag.c_str());

		id2dag[id] = dag;
		dag2id[dag] = id;
	}

	return 0;
}

// Generate a random SID
static char *_NewSID(char *buf, unsigned len)
{
	// FIXME: this is a stand-in function until we get certificate based names
	//
	// note: buf must be at least 45 characters long
	// (longer if the XID type gets longer than 3 characters)
	if (len < SID_SIZE) {
		WARNING("buf is only %d chars long, it needs to be %d. Truncating...\n", len, SID_SIZE);
	}

	snprintf(buf, len, "SID:44444ff0000000000000000000000000%08x", rand());

	return buf;
}

// convert a IPv4 sockaddr into an id string in the form of A.B.C.D-port
static char *_IDstring(char *s, const struct sockaddr_in* sa)
{
	char ip[ID_LEN];

	inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);

	// make an id from the ip address and port
	sprintf(s, "%s-%u", ip, ntohs(sa->sin_port));
	return s;
}

// map from IP to XIA
static int _i2x(struct sockaddr_in *sin, sockaddr_x *sax)
{
	char s[ID_LEN];
	int rc = 0;

	id2dag_t::iterator it = id2dag.find(_IDstring(s, sin));

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
static int _x2i(sockaddr_x *sax, sockaddr_in *sin)
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

		inet_aton(id, &sin->sin_addr);
		sin->sin_port = atoi(p);
		sin->sin_family = AF_INET;

	} else {
		rc = -1;
		MSG("No mapping for %s\n", g.dag_string().c_str());
	}
	
	return rc;
}

// create a dag with sid for this sockaddr and register it with
// the xia name server. Also create a mapping to go from ip->xia and xia-ip
static int _Register(const struct sockaddr *addr, socklen_t len)
{
	char id[ID_LEN];
	char sid[SID_SIZE];
	struct sockaddr_in sa;
	struct addrinfo *ai;

	memcpy(&sa, addr, len);

	// create a DAG for this host in the form of "(4ID) AD HID SID"
	Xgetaddrinfo(NULL, _NewSID(sid, sizeof(sid)), NULL, &ai);

	// register it in the name server with the ip-port id
	XregisterName(_IDstring(id, &sa), (sockaddr_x*)ai->ai_addr);

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

	_IDstring(id, addr);

	if (_NegativeLookup(id)) {
		return -1;
	}

	MSG("Looking up mapping for %s in the nameserver\n", id);

	if (XgetDAGbyName(id, sax, &len) < 0) {
		WARNING("Mapping server lookup failed for %s\n", id);

		failedLookups[id] = time(NULL);
		rc = -1;

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

		if (FORCE_XIA()) {
			// create a new fake IP address/port  to map to
			_GetIP(&sax, (struct sockaddr_in*)ipaddr, NULL, 0);

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

	TRACE();
	if (shouldWrap(fd) ) {
		XIAIFY();

		if (FORCE_XIA()) {
			// create a mapping from IP/port to a dag and register it
			_Register(addr, len);

			// convert the sockaddr to a sockaddr_x
			_i2x((struct sockaddr_in*)addr, &sax);
			addr = (struct sockaddr*)&sax;
		}

		rc = Xbind(fd, addr, len);

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
	
	TRACE();
	if (shouldWrap(fd)) {
		XIAIFY();

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
				
			_IDstring(id, (sockaddr_in*)addr);
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

	TRACE();
	va_start(args, cmd);

	if (isXsocket(fd)) {
		XIAIFY();
			rc = Xfcntl(fd, cmd, args);
	
	} else {
		NOXIA();
		rc = __real_fcntl(fd, cmd, args);
	}

	va_end(args);
	return rc;
}

void freeaddrinfo (struct addrinfo *ai)
{
	TRACE();
	// there currently isn't any new XIA functionality for this call
	return __real_freeaddrinfo(ai);
}

const char *gai_strerror (int ecode)
{
	TRACE();
	// there currently isn't any new XIA functionality for this call
	return __real_gai_strerror(ecode);
}

int getaddrinfo (const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **pai)
{
	int rc = -1;
	int tryxia = 1;
	int trynormal = 1;

	TRACE();

	if (hints) {
		// let's see if we can steal it
		if (hints->ai_family != AF_UNSPEC && 
			hints->ai_family != AF_INET && 
			hints->ai_family != AF_XIA) {
		
			tryxia = 0;
		}
	}

	if (FORCE_XIA() && tryxia) {
		sockaddr_x sax;
		char s[64];
		int socktype = 0;
		int protocol = 0;
		int family = AF_INET;
		int flags = 0;
		int port;
		socklen_t len = sizeof(sax);
		struct sockaddr *sa;

		if (hints) {
			if (hints->ai_flags != 0) {
				WARNING("Flags to getaddrinfo are not currently implemented.\n");
			}
			socktype = hints->ai_socktype;
			protocol = hints->ai_protocol;
		}

		if (service) {
			port = strtol(service, NULL, 10);
			if (errno == EINVAL) {
				// service was not an integer

				// FIXME:is this threadsafe???
				struct servent *serv = getservbyname(service, NULL);
				if (serv) {
					port = serv->s_port;
				} else {
					port = _NewPort();
				}
				endservent();
			}
		} else {
			port = _NewPort();
		}


		if (strcmp(name, "0.0.0.0") == 0) {
			// caller wants a sockaddr for itself
			// create a fake ip address
			// fill in sax with the bogus ip address

			sa = (struct sockaddr *)calloc(sizeof(struct sockaddr), 1);
			_GetIP(NULL, (struct sockaddr_in*)sa, NULL, htons(port));

			rc = 0;
		} else {

			sprintf(s, "%s-%d", name, port);

			if (_NegativeLookup(s)) {
				// we've already failed to look up this name
				errno = EAI_NONAME;
				return -1;
			}
			MSG("addr=%s\n", s);

//			if (_Lookup(s, NULL, &sax) < 0)
//				return -1;
			if (XgetDAGbyName(s, &sax, &len) < 0) {
				MSG("name lookup failed for %s\n", s);

				failedLookups[s] = time(NULL);
				errno = EAI_NONAME;
				return -1;
			}

			sa = (struct sockaddr *)calloc(sizeof(struct sockaddr), 1);
//			sockaddr_in *sin = (struct sockaddr_in *)sa;
			_GetIP(&sax, (sockaddr_in *)sa, name, htons(port));
//			inet_aton(name, &sin->sin_addr);
//			sin->sin_family = AF_INET;
//			sin->sin_port = htons(port);

			Graph g(&sax);
			std::string dag = g.dag_string();
//			id2dag[s] = dag;
//			dag2id[dag] = s;

MSG("found name\n%s\n", dag.c_str());
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

		*pai = ai;
		rc = 0;

	} else {
		// try to determine if this is an XIA address lookup
		if (hints) {
			// they specified hints, see if the family is XIA, or unspecified
			if (hints->ai_family == AF_UNSPEC) {
				MSG("family is unspec\n");
				tryxia = 1;
			} else if (hints->ai_family == AF_XIA) {
				MSG("family is XIA\n");
				tryxia = 1;
				trynormal = 0;
					tryxia = 0;
				tryxia = 0;
			}
		}

		if (tryxia) {
			XIAIFY();
			MSG("looking up name in XIA\n");
			rc = Xgetaddrinfo(name, service, hints, pai);
			MSG("Xgetaddrinfo returns %d\n", rc);
		}

		// XIA lookup failed, fall back to
		if (trynormal && rc < 0) {
			NOXIA();
			MSG("looking up name normally\n");
			rc = __real_getaddrinfo(name, service, hints, pai);
		}
	}

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
			_GetIP(&sax, (sockaddr_in*)addr, NULL, 0);
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

		rc = Xgetsockname(fd, (struct sockaddr*)&sax, &slen);

		if (_x2i(&sax, (struct sockaddr_in*)addr) < 0) {
			_GetIP(&sax, (sockaddr_in*)addr, NULL, 0);
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
		rc =  Xgetsockopt(fd, optname, optval, optlen);

		if (rc < 0) {
			// TODO: add code here to return success for options we can safely ignore
		}

	} else {
		NOXIA();
		rc = __real_getsockopt(fd, level, optname, optval, optlen);
	}
	return rc;
}

extern "C" int ioctl(int d, int request, ...)
{
	int rc;
	va_list args;

	TRACE();
	va_start(args, request);

	if (isXsocket(d)) {
		// Not sure what ioctl requests should work on an xsocket
		// just flag it for now
		ALERT();
		rc = -1;
	} else {
		NOXIA();
		rc = __real_ioctl(d, request, args);
	}
	va_end(args);
	return rc;
}

int listen(int fd, int n)
{
	TRACE();
	if (isXsocket(fd)) {
		// XIA doesn't have a listen function yet, so just return success
		SKIP();
		return 0;

	} else {
		NOXIA();
		return __real_listen(fd, n);
	}
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int rc;
	TRACE();
	// Let Xpoll do all the work of figuring out what fds we are handling
	MSG("Xpoll: %zu %d %08x\n", nfds, fds[0].fd, fds[0].events);
	rc = Xpoll(fds, nfds, timeout);
	MSG("Xpoll returns %d %d %08x\n", rc, fds[0].fd, fds[0].revents);
	return rc;
}

ssize_t read(int fd, void *buf, size_t count)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		rc = Xrecv(fd, buf, count, 0);

	} else {
		NOXIA();
		rc = __real_read(fd, buf, count);
	}
	return rc;
}

ssize_t recv(int fd, void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		rc = Xrecv(fd, buf, n, flags);

		char *c = (char *)buf;
		c[rc] = 0;
		MSG("%s\n", c);

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
	struct sockaddr *ipaddr;
	socklen_t slen;

	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			slen = (socklen_t)sizeof(sax);
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
			*addr_len = sizeof(addr);
			// FIXME we should check to see if addr and addr_len are valid
		}  

		rc = Xrecvfrom(fd, buf, n, flags, addr, &slen);

		if (rc >= 0 && FORCE_XIA()) {
			// convert the sockaddr to a sockaddr_x
			if (_x2i(&sax, (struct sockaddr_in*)ipaddr) < 0) {
				// we don't have a mapping for this yet, create a fake IP address
				_GetIP(&sax, (sockaddr_in *)ipaddr, NULL, 0);
			}
		}	

	} else {
		NOXIA();
		rc = __real_recvfrom(fd, buf, n, flags, addr, addr_len);
	}

	return rc;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	TRACE();
	// Let Xselect do all the work of figuring out what fds we are handling
	return Xselect(nfds, readfds, writefds, exceptfds, timeout);
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();

		// FIXME: debugging code
		char *p = (char *)buf;
		p[n] = 0;
		MSG("%s\n", p);
		// end debug
		
		rc = Xsend(fd, buf, n, flags);


	} else {
		NOXIA();
		rc = __real_send(fd, buf, n, flags);
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

		if (FORCE_XIA()) {

			if (_Lookup((sockaddr_in *)addr, &sax) != 0) {
				char id[ID_LEN];

				_IDstring(id, (sockaddr_in*)addr);
				WARNING("Unable to lookup %s\n", id);
				errno = EINVAL;	// FIXME: is there a better return we can use here?
				return -1;
			}

			addr_len = sizeof(sax);
			addr = (struct sockaddr*)&sax;
		}

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
		rc =  Xsetsockopt(fd, optname, optval, optlen);

		if (rc < 0) {
			// TODO: add code here to return success for options we can safely ignore
		}

	} else {
		NOXIA();
		rc = __real_setsockopt(fd, level, optname, optval, optlen);
	}

	return rc;
}

int socket(int domain, int type, int protocol)
{
	int fd;
	TRACE();

	if ((domain == AF_XIA || (domain == AF_INET && FORCE_XIA()))) {
		XIAIFY();

		if (protocol != 0) {
			MSG("Caller specified protocol %d, resetting to 0\n", protocol);
			protocol = 0;
		}

		fd = Xsocket(AF_XIA, type, protocol);

	} else {
		NOXIA();
		fd = __real_socket(domain, type, protocol);
	}
	return fd;
}

int socketpair(int domain, int type, int protocol, int fds[2])
{
	TRACE();
	if (domain == AF_XIA || (domain == AF_INET && FORCE_XIA())) {
		XIAIFY();
		fds[0] = socket(domain, type, protocol);
		fds[1] = socket(domain, type, protocol);

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

ssize_t write(int fd, const void *buf, size_t count)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		rc = Xsend(fd, buf, count, 0);

	} else {
		NOXIA();
		rc = __real_write(fd, buf, count);
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
	MSG("%zu %d %08x\n", nfds, fds[0].fd, fds[0].events);
	rc = Xpoll(fds, nfds, timeout);
	MSG("Xpoll returns %d %d %08x\n", rc, fds[0].fd, fds[0].revents);
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

	// There's no state to work with so we have to assume
	// everything is for XIA

	// FIXME: add code here to map between IPv4 and XIA
	return __real_gethostbyname(name);
}

int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();

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

ssize_t recvmsg(int fd, struct msghdr *message, int flags)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		NOXIA();
		return __real_recvmsg(fd, message, flags);
	}
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		NOXIA();
		return __real_sendmsg(fd, message, flags);
	}
}
