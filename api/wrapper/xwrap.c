
//#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "dagaddr.hpp"
#include <map>

#define TRACE()		{if (_log_trace)   fprintf(_log, "xwrap: %s\r\n", __FUNCTION__);}
#define STOCK()		{if (_log_trace)   fprintf(_log, "xwrap: %s informational tracing only\r\n", __FUNCTION__);}

#define MSG(...)	{if (_log_info)    fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}

#define XIAIFY()	{if (_log_wrap)     fprintf(_log, "xwrap: %s redirected to XIA\r\n", __FUNCTION__);}
#define NOXIA()		{if (_log_wrap)     fprintf(_log, "xwrap: %s used normally\r\n", __FUNCTION__);}
#define SKIP()		{if (_log_wrap)     fprintf(_log, "xwrap: %s not required/supported in XIA (no-op)\r\n", __FUNCTION__);}

#define ALERT()		{if (_log_warning) fprintf(_log, "xwrap: ALERT!!!, %s is not implemented in XIA!\r\n", __FUNCTION__);}
#define WARNING(...)	{if (_log_warning) fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}

#define FORCE_XIA() (_pure_xia != 0 ? 1 : 0)

#define NEG_LOOKUP_MAX	15
#define SID_SIZE 45 // (40 byte XID + 4 bytes SID: + null terminator)

/*
** Declare a typedef and a function pointer definition that uses it.
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

/*
**  using LD_PRELOAD method, variables for each overridden function
*/
// socket I/O or potential socket I/O calls
DECLARE(int, socket, int domain, int type, int protocol);
DECLARE(int, socketpair, int domain, int type, int protocol, int fds[2]);
DECLARE(int, bind, int fd, const struct sockaddr *addr, socklen_t len);
DECLARE(int, getsockname, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(int, connect, int fd, const struct sockaddr *addr, socklen_t len);
DECLARE(int, getpeername, int fd, struct sockaddr *addr, socklen_t *len);
DECLARE(ssize_t, send, int fd, const void *buf, size_t n, int flags);
DECLARE(ssize_t, recv, int fd, void *buf, size_t n, int flags);
DECLARE(ssize_t, sendto, int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len);
DECLARE(ssize_t, recvfrom, int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, getsockopt, int fd, int level, int optname, void *optval, socklen_t *optlen);
DECLARE(int, setsockopt, int fd, int level, int optname, const void *optval, socklen_t optlen);
DECLARE(int, listen, int fd, int n);
DECLARE(int, accept, int fd, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, close, int fd);
DECLARE(int, ioctl, int d, int request, ...);
DECLARE(int, fcntl, int fd, int cmd, ...);
DECLARE(int, getaddrinfo, const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
DECLARE(void, freeaddrinfo, struct addrinfo *ai);
DECLARE(const char *, gai_strerror, int ecode);

DECLARE(ssize_t, read, int fd, void *buf, size_t count);
DECLARE(ssize_t, write, int fd, const void *buf, size_t count);
DECLARE(int, select, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
DECLARE(int, poll, struct pollfd *fds, nfds_t nfds, int timeout);
DECLARE(int, __poll_chk, struct pollfd *fds, nfds_t nfds, int timeout, __SIZE_TYPE__ __fdslen);

// not ported to XIA, remapped for warning purposes
DECLARE(ssize_t, sendmsg, int fd, const struct msghdr *message, int flags);
DECLARE(ssize_t, recvmsg, int fd, struct msghdr *message, int flags);
DECLARE(struct hostent *,gethostbyaddr, const void *addr, socklen_t len, int type);
DECLARE(struct hostent *,gethostbyname, const char *name);
DECLARE(int, gethostbyaddr_r, const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, gethostbyname_r, const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, getservbyname_r, const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(int, getservbyport_r, int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(struct servent*, getservbyname, const char *name, const char *proto);
DECLARE(struct servent*, getservbyport, int port, const char *proto);
DECLARE(int, getnameinfo, const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags);
 

typedef std::map<std::string, std::string> ip2dag_t;
typedef std::map<std::string, std::string> dag2ip_t;
typedef std::map<std::string, time_t> failed_t;

ip2dag_t ip2dag;
dag2ip_t dag2ip;
failed_t failedLookups;

// used for creating the fake IP addresses
int low_ip = 1;
int high_ip = 0;

static __thread int _wrap_socket = 0;	// one copy of this per thread, controls
										//  whether we should wrap the socket call
										//  or not when running in pure xia mode.

//****************************************************************************
// set up logging parameters
//
static int _log_trace = 0;
static int _log_warning = 0;
static int _log_info = 0;
static int _log_wrap = 0;
static FILE *_log = NULL;

//****************************************************************************
// If true treats all sockets as XIA sockets and does funky sockaddr mapping
// so applications can hopefully be totally unchanged, but use XIA under the
// covers
//
int _pure_xia = 0;

void __xwrap_setup()
{
	srand(time(NULL));
//	_wrap_socket = 1;

	low_ip = (rand() % 253) + 1;
	high_ip = rand() % 254;

	if (getenv("XWRAP_XIA") != NULL)
		_pure_xia = 1;

	if (getenv("XWRAP_TRACE") != NULL)
		_log_trace = 0;
	if (getenv("XWRAP_VERBOSE") != NULL)
		_log_info = _log_wrap = _log_warning = 1;
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
}


/******************************************************************************
**
** Called at library load time to initialize the function pointers
**
******************************************************************************/
void __attribute__ ((constructor)) xwrap_init(void)
{
	__xwrap_setup();

	if (_log_info || _log_warning || _log_wrap || _log_trace) {
		fprintf(_log, "loading XIA wrappers (created: %s)\n", __DATE__);
		if (FORCE_XIA())
			fprintf(_log, "remapping all socket IO automatically into XIA\n");
	}

	GET_FCN(socket);
	GET_FCN(socketpair);
	GET_FCN(bind);
	GET_FCN(getsockname);
	GET_FCN(connect);
	GET_FCN(getpeername);
	GET_FCN(send);
	GET_FCN(recv);
	GET_FCN(sendto);
	GET_FCN(recvfrom);
	GET_FCN(sendmsg);
	GET_FCN(recvmsg);
	GET_FCN(getsockopt);
	GET_FCN(setsockopt);
	GET_FCN(listen);
	GET_FCN(accept);
	GET_FCN(close);
	GET_FCN(ioctl);
	GET_FCN(fcntl);
	GET_FCN(gethostbyaddr);
	GET_FCN(gethostbyname);
	GET_FCN(gethostbyaddr_r);
	GET_FCN(gethostbyname_r);
	GET_FCN(getservbyname);
	GET_FCN(getservbyport);
	GET_FCN(getservbyname_r);
	GET_FCN(getservbyport_r);
	GET_FCN(getaddrinfo);
	GET_FCN(freeaddrinfo);
	GET_FCN(gai_strerror);
	GET_FCN(getnameinfo);

	GET_FCN(read);
	GET_FCN(write);
	GET_FCN(select);
	GET_FCN(poll);
	GET_FCN(__poll_chk);
}


/******************************************************************************
**
** Helper functions
**
******************************************************************************/

// call into the Xsockets API to see if the fd is associated with an Xsocket
#define isXsocket(s)	 (getSocketType(s) != -1)
#define markWrapped(s)	 {setWrapped(s, 1);}
#define markUnwrapped(s) {setWrapped(s, 0);}
#define shouldWrap(s)	 (isXsocket(s) && !isWrapped(s))
//#define shouldWrap(s)	 (isXsocket(s))


// FIXME: need a different name for this so it doesn't collide with above.
int _GetSocketType(int type)
{
	/*
	** XSOCK_STREAM, XSOCK_DGRAM, XSOCK_RAW have the same values
	** as the matching XSOCK constants.
	**
	** FIXME: XSOCK_CHUNK == 4 == SOCK_RDM
	*/
	switch(type & 0xf) {
		case XSOCK_STREAM:
		case XSOCK_DGRAM:
		case XSOCK_CHUNK:
		case XSOCK_RAW:
			return type;
		default:
			MSG("%d is not a valid XSOCKET type\n", type);
			break;
	}
	return XSOCK_INVALID;
}

int _NewPort()
{
	return 1024 + (rand() % (32767 - 1024));
}

// Create a new fake IP address to associate with the given DAG
int _GetIP(sockaddr_x *sax, struct sockaddr_in *sin, const char *addr, int port)
{
	char s[64];
	char s1[64];

	if (port == 0)
		port = _NewPort();

	if (!addr) { 
		sprintf(s, "169.254.%d.%d", high_ip, low_ip);
		addr = s;
	}
	inet_aton(addr, &sin->sin_addr);

	sin->sin_family = AF_INET;
	sin->sin_port = port;

	sprintf(s1, "%s-%d", addr, ntohs(sin->sin_port));

	if (sax) {
		Graph g(sax);

		std::string dag = g.dag_string();

		ip2dag[s1] = dag;
		dag2ip[dag] = s1;
	}

	// FIXME: do we want to increment thid each time or jst set it once for each app?
	// bump the ip address for next time
	low_ip++;
	if (low_ip == 254) {
		low_ip = 1;
		high_ip++;
		if (high_ip == 255) {
			high_ip = 0;
		}
	}

	return 0;
}

// Generate a random SID. This will need to be modified when we 
// add in intrinsic security support
char *_RandomSID(char *buf, unsigned len)
{
	// This is a stand-in function until we get certificate based names
	//
	// note: buf must be at least 45 characters long
	// (longer if the XID type gets longer than 3 characters)
	if (len < SID_SIZE) {
		WARNING("buf is only %d chars long, it needs to be %d. Truncating...\n", len, SID_SIZE);
	}

	snprintf(buf, len, "SID:44444ff0000000000000000000000000%08x", rand());

	return buf;
}

// convert a IPv4 sock addr into a sting in the form of A.B.C.D-port
char *_IpString(char *s, const struct sockaddr_in* sa)
{
	// FIXME: this doesn't seem threadsafe
	char *ip = inet_ntoa(sa->sin_addr);

	// make a name from the ip address and port
	sprintf(s, "%s-%u", ip, ntohs(sa->sin_port));
//	MSG("%s\n", s);
	return s;
}

// map from IP to XIA
int _i2x(struct sockaddr_in *sin, sockaddr_x *sax)
{
	char s[64];
	int rc = 0;

	ip2dag_t::iterator it = ip2dag.find(_IpString(s, sin));

	if (it != ip2dag.end()) {

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
int _x2i(sockaddr_x *sax, sockaddr_in *sin)
{
	// FIXME: this depends on the created dag string always looking the same!
	char name[64];
	Graph g(sax);
	int rc = 0;

	dag2ip_t::iterator it = dag2ip.find(g.dag_string());

	if (it != dag2ip.end()) {
		strcpy(name, it->second.c_str());
		MSG("Found: %s\n", name);

		// chop name into ip address and port
		char *p = strchr(name, '-');
		*p++ = 0;

		inet_aton(name, &sin->sin_addr);
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
int _Register(const struct sockaddr *addr, socklen_t len)
{
	char name[32];
	char sid[64];
	struct sockaddr_in sa;
	struct addrinfo *ai;

	memcpy(&sa, addr, len);

	// create a DAG (4ID) AD HID SID
	_wrap_socket = 0;	// Xgetaddrinfo calls xsocket, so we want to let its internal socket call go through
	Xgetaddrinfo(NULL, _RandomSID(sid, sizeof(sid)), NULL, &ai);
	_wrap_socket = 1;

	XregisterName(_IpString(name, &sa), (sockaddr_x*)ai->ai_addr);

	Graph g((sockaddr_x*)ai->ai_addr);

	std::string dag = g.dag_string();

	ip2dag[name] = dag;
	dag2ip[dag] = name;

	return 1;
}

int _FailedHit(std::string id)
{
	int rc = 0;
	failed_t::iterator it = failedLookups.find(id);

	if (it != failedLookups.end()) {
		time_t t = it->second;
		if (time(NULL) - t >= NEG_LOOKUP_MAX) {
			failedLookups.erase(it);
			MSG("clearing failed lookup entry for %s\n", id.c_str());
		} else {
			rc = -1;
		}
	}

	return rc;
}

int _Lookup(const struct sockaddr_in *addr, sockaddr_x *sax)
{
	int rc = 0;
	char s[64];
	socklen_t len = sizeof(sockaddr_x);

	if (_i2x((struct sockaddr_in*)addr, sax) == 0)
		return rc;

	_IpString(s, addr);

	if (_FailedHit(s))
		return -1;

	MSG("Looking up mapping for %s\n", s);

	if (XgetDAGbyName(s, sax, &len) < 0) {
		WARNING("Mapping lookup failed for %s\n", s);

		failedLookups[s] = time(NULL);
		rc = -1;
	} else {

		Graph g(sax);

		std::string dag = g.dag_string();

		ip2dag[s] = dag;
		dag2ip[dag] = s;
	}

	return rc;
}

/******************************************************************************
**
** FUNCTION REMAPPINGS START HERE
**
******************************************************************************/

int socket(int domain, int type, int protocol)
{
	int fd;
	TRACE();

//	MSG("SOCKET t:%d p:%d %d %d\n", type, protocol, SOCK_STREAM, SOCK_DGRAM);
	if ((domain == AF_XIA || (domain == AF_INET && FORCE_XIA())) && _wrap_socket) {
//	if ((domain == AF_XIA || (domain == AF_INET && FORCE_XIA()))) {
		XIAIFY();

		if (protocol != 0) {
			MSG("Caller specified protocol %d, resetting to 0\n", protocol);
			protocol = 0;
		}

		_wrap_socket = 0;
		fd = Xsocket(AF_XIA, type, protocol);
		_wrap_socket = 1;
	} else {
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
			markWrapped(fds[0]);
			markWrapped(fds[1]);
			Xclose(fds[0]);
			Xclose(fds[1]);
			markUnwrapped(fds[0]);
			markUnwrapped(fds[1]);
			errno = e;
			return -1;
		}
	} else {
		return __real_socketpair(domain, type, protocol, fds);
	}
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	int rc;
	sockaddr_x sax;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			// FIXME: this will currently fail if the application creates the sockaddr
			// internally instead of getting it from getaddrinfo, we probably need
			// to add a name lookup if the mapping lookup fails

			// convert the sockaddr to a sockaddr_x
			_i2x((struct sockaddr_in*)addr, &sax);
			addr = (struct sockaddr*)&sax;
		}

		markWrapped(fd);
		rc= Xconnect(fd, addr, len);
		markUnwrapped(fd);
		MSG("rc = %d\n", rc);
	} else {
		rc = __real_connect(fd, addr, len);
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

		markWrapped(fd);
		rc= Xbind(fd, addr, len);
		markUnwrapped(fd);

	} else {
		rc = __real_bind(fd, addr, len);
	}

	return rc;
}

int accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
	int rc;
	sockaddr_x sax;
	struct sockaddr *ipaddr;
	socklen_t xlen;

	MSG("%d\n", fd);
	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		xlen = sizeof(sax);
		markWrapped(fd);
		_wrap_socket = 0;
		rc = Xaccept(fd, addr, &xlen);
		_wrap_socket = 1;
		markUnwrapped(fd);

		if (FORCE_XIA()) {
			// create a new fake IP address/port  to map to
			_GetIP(&sax, (struct sockaddr_in*)ipaddr, NULL, 0);

			// convert the sockaddr_x to a sockaddr
			_x2i(&sax, (struct sockaddr_in*)ipaddr);
		}		

	} else {
		rc = __real_accept(fd, addr, addr_len);
	}
	return rc;
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xsend(fd, buf, n, flags);
		markUnwrapped(fd);
	} else {
		rc = __real_send(fd, buf, n, flags);
	}
	return rc;
}

ssize_t recv(int fd, void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	//MSG("recv %d %ld\n", fd, n);

	if (shouldWrap(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xrecv(fd, buf, n, flags);
		markUnwrapped(fd);
	} else {
		rc = __real_recv(fd, buf, n, flags);
	}

	//MSG("recv exiting %d %s\n", rc, strerror(errno));
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
				char ips[64];
				_IpString(ips, (sockaddr_in*)addr);
				WARNING("Unable to lookup %s\n", ips);
				errno = EINVAL;	// FIXME: is there a better return we can use here?
				return -1;
			}

			addr_len = sizeof(sax);
			addr = (struct sockaddr*)&sax;
		}

		markWrapped(fd);
		rc = Xsendto(fd, buf, n, flags, addr, addr_len);
		MSG("Xsendto returned %d while sending %ld bytes on fd %d\n", rc, n, fd);
		markUnwrapped(fd);

	} else {
		rc = __real_sendto(fd, buf, n, flags, addr, addr_len);
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

		markWrapped(fd);
		rc = Xrecvfrom(fd, buf, n, flags, addr, &slen);
		markUnwrapped(fd);

		if (FORCE_XIA()) {
			// convert the sockaddr to a sockaddr_x
			if (_x2i(&sax, (struct sockaddr_in*)ipaddr) < 0) {
				// we don't have a mapping for this yet, create a fake IP address
				_GetIP(&sax, (sockaddr_in *)ipaddr, NULL, 0);
			}
		}	

	} else {
		rc = __real_recvfrom(fd, buf, n, flags, addr, addr_len);
	}

	return rc;
}

ssize_t read(int fd, void *buf, size_t count)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		markWrapped(fd);
		rc = Xrecv(fd, buf, count, 0);
		markUnwrapped(fd);

	} else {
		rc = __real_read(fd, buf, count);
	}
	return rc;
}

ssize_t write(int fd, const void *buf, size_t count)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();

		markWrapped(fd);
		rc = Xsend(fd, buf, count, 0);
		markUnwrapped(fd);

	} else {
		rc = __real_write(fd, buf, count);
	}
	return rc;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	int rc;
	int xify = FALSE;
	static int last = 0;
	static int count = 0;
	int t = time(NULL);


	if ((last == 0) || (t - last > 3)) {
		if (last == 0)
			count = 1;
		last = t;
		MSG("called %d times\n", count);
		count = 1;
	//	TRACE();
	} else {
		count++;
	}

	// don't trace as the screen will fill with select messages from click_get
	//	TRACE();

	// FIXME: we are in trouble if the fdset contains both socket and file descriptors
	// not sure how to handle it other than to throw a warning message right now

	if (_wrap_socket == 0) {
		xify = FALSE;
	} else {
		// ugh - have to walk the fdsets to see if there's a socket in there
		
		for (int i = 0; i < nfds; i++) {

			int found;

			found = FALSE;
			if (readfds && FD_ISSET(i, readfds))
				found = TRUE;
			else if (writefds && FD_ISSET(i, writefds))
				found = TRUE;
			else if (exceptfds && FD_ISSET(i, exceptfds))
				found = TRUE;


			if (found) {
				if (getSocketType(i) != XSOCK_INVALID) {
//					MSG("select found socket %d\n", i);
					if (!isAPI(i)) {
//						MSG("it is not an API socket\n");
						xify = TRUE;
					}
					else {
//						MSG("it is an API socket!\n");
					}
					break;
				}
			}
		}

		// if xify is true, the selector set contains xsockets, but might be
		// called by the internal select used in click_get so we need to do
		// another check to see if we should xiaify it
	}


	if (xify) {
		XIAIFY();
	
		// select may cover multiple sockets so we can't call setWrapped here
		//  it also uses multiple socket calls internally, so set the wrap_socket
		//  flag so it can create a normal socket
		_wrap_socket = 0;
		rc = Xselect(nfds, readfds, writefds, exceptfds, timeout);
		_wrap_socket = 1;

	} else {
		rc = __real_select(nfds, readfds, writefds, exceptfds, timeout);
	}

	return rc;
}

int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, __SIZE_TYPE__ __fdslen)
{
	TRACE();

	MSG("POLL CHECK ****************************************************\n");
	return __real___poll_chk(fds, nfds, timeout, __fdslen);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	int rc;
	bool xify = FALSE;
	bool normal = FALSE;

	TRACE();

	// TODO: add code to verify parameters and return proper error codes

	// allocate 
	struct pollfd *xfds = (struct pollfd*)calloc(sizeof(pollfd), nfds);
	struct pollfd *ffds = (struct pollfd*)calloc(sizeof(pollfd), nfds);

	memcpy(xfds, fds, nfds * sizeof(struct pollfd));
	memcpy(ffds, fds, nfds * sizeof(struct pollfd));

//MSG("POLL**********************************************\n");

	for (unsigned i = 0; i < nfds; i++) {
		if (getSocketType(fds[i].fd) != XSOCK_INVALID) {
//			MSG("We found an XIA socket %d\n", fds[i].fd);
			xify = TRUE;

			// stop on the descriptor in the fd set
			ffds[i].fd = -ffds[i].fd;
		} else {
//			MSG("We found a file descriptor %d\n", fds[i].fd);
			normal = TRUE;

			// stop on the descriptor in the xia set
			xfds[i].fd = -xfds[i].fd;
		}
		fds[i].revents = xfds[i].revents = ffds[i].revents = 0;
	}

	if (xify && !normal) {
//		MSG("Doing XIA only poll\n");
		_wrap_socket = 0;
		rc = Xpoll(fds, nfds, timeout);
		_wrap_socket = 1;
	} else if (normal && !xify) {
//		MSG("Doing fd only poll\n");
		rc = __real_poll(fds, nfds, timeout);
	} else {
		// we need to do both kinds of polls
		int rcx, rcf;

//		MSG("doing dual poll\n");

		if (timeout == 0) {
//			MSG("zero timeout poll\n");
			// just check and return
			_wrap_socket = 0;
			rcx = Xpoll(xfds, nfds, 0);
			_wrap_socket = 1;
			rcf = __real_poll(ffds, nfds, 0);

			rc = rcx + rcf;
		} else {
			struct timeval start, now;
			int delay = 100;	// find optimal value for this

//			MSG("poll timeout = %d\n", timeout); 

			// ugh, we have to loop and do short timeouts on each set of pollfds
			// it's ugly, but I don't see any other way to handle it
			// FIXME: consider spinning up 2 threads to do it

//			MSG("start = %d:%d\n", (unsigned)start.tv_sec, (unsigned)start.tv_usec);

			gettimeofday(&start, NULL);
			for (;;) {

//				MSG("calling real poll\n");
				rc = __real_poll(fds, nfds, delay);
//				MSG("real poll returns %d\n", rc);
				if (rc != 0)
					break;

//				MSG("calling xpoll\n");
				_wrap_socket = 0;
				rc = Xpoll(xfds, nfds, delay * 100);
				_wrap_socket = 1;
//				MSG("Xpoll returns %d\n", rc);
				if (rc != 0)
					break;

				if (timeout > 0) {
					gettimeofday(&now, NULL);
//					MSG("current = %d:%d\n", (unsigned)now.tv_sec, (unsigned)now.tv_usec);
					int secs = now.tv_sec - start.tv_sec;
					int usecs = now.tv_usec - start.tv_usec;
					int elapsed = (secs * 1000) + (usecs / 1000); 

//					MSG("elapsed = %d\n", elapsed);

					if (elapsed >= timeout) {
						// we didn't find anything, time to go
						MSG("Poll timed out\n");
						rc = 0;
						break;
					}
				}
			}
		}

		// put the results back into the callers pollfd list
		if (rcx > 0) {
			for (unsigned i = 0; i < nfds; i++) {
				if (xfds[i].revents != 0 && xfds[i].revents != POLLNVAL) {
//					MSG("Got an XIA hit on fd %d:%d\n", xfds[i].fd, xfds[i].revents);
					fds[i].revents = xfds[i].revents;
				}
			}
		}

		if (rcf > 0) {
			for (unsigned i = 0; i < nfds; i++) {
				if (xfds[i].revents != 0) {
//					MSG("Got a normal hit on fd %d\n", fds[i].fd);
					fds[i].revents = ffds[i].revents;
				}
			}
		}
	}

	return rc;
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		return __real_sendmsg(fd, message, flags);
	}
}

ssize_t recvmsg(int fd, struct msghdr *message, int flags)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		return __real_recvmsg(fd, message, flags);
	}
}

int getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
	int rc;
	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc =  Xgetsockopt(fd, optname, optval, optlen);
		markUnwrapped(fd);

		if (rc < 0) {
			// TODO: add code here to return success for options we can safely ignore
		}

	} else {
		rc = __real_getsockopt(fd, level, optname, optval, optlen);
	}
	return rc;
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int rc;
	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc =  Xsetsockopt(fd, optname, optval, optlen);
		markUnwrapped(fd);

		if (rc < 0) {
			// TODO: add code here to return success for options we can safely ignore
		}

	} else {

		rc = __real_setsockopt(fd, level, optname, optval, optlen);
	}

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
		return __real_listen(fd, n);
	}
}

int close(int fd)
{
	int rc;
	TRACE();
	if (shouldWrap(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xclose(fd);
		markUnwrapped(fd);

		// FIXME: we should clean up entries from the dag2ip and ip2dag maps here

	} else {
		rc = __real_close(fd);
	}
	return rc;
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;
	struct sockaddr *ipaddr;
	sockaddr_x sax;
	socklen_t oldlen;

	TRACE();
	if (shouldWrap(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			oldlen = *len;
			*len = sizeof(sax);
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		if (*len < sizeof(sockaddr_x)) {
			WARNING("not enough room for the XIA sockaddr! have %d, need %ld\n", *len, sizeof(sockaddr_x));
		}

		markWrapped(fd);
		rc = Xgetsockname(fd, addr, len);
		markUnwrapped(fd);

		// FIXME: WHAT DO WE DO HERE IF THE MAPPING LOOKUP FAILS???
		if (FORCE_XIA()) {
			*len = oldlen;
			// convert the sockaddr to a sockaddr_x
			_x2i(&sax, (struct sockaddr_in*)ipaddr);
		}			

	} else {
		rc = __real_getsockname(fd, addr, len);
	}
	return rc;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;
	struct sockaddr *ipaddr;
	sockaddr_x sax;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		if (*len < sizeof(sockaddr_x))
			WARNING("not enough room for the XIA sockaddr! have %d, need %ld\n", *len, sizeof(sockaddr_x));

		if (FORCE_XIA()) {
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		markWrapped(fd);
		rc = Xgetpeername(fd, addr, len);
		markUnwrapped(fd);

		if (FORCE_XIA()) {
			// convert the sockaddr to a sockaddr_x
			if (_x2i(&sax, (struct sockaddr_in*)ipaddr) < 0) {
				_GetIP(&sax, (sockaddr_in*)ipaddr, NULL, 0);
			}
		}	

	} else {
		return __real_getpeername(fd, addr, len);
	}
	return rc;
}


int ioctl(int d, int request, ...)
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

int fcntl (int fd, int cmd, ...)
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

int getaddrinfo (const char *name, const char *service, const struct addrinfo *hints, struct addrinfo **pai)
{
	int rc = -1;
	int tryxia = 1;
	int trynormal = 1;

	TRACE();

	if (hints) {
		// see what type of address they want
		if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET && hints->ai_family != AF_XIA)
			tryxia = 0;
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
			if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) {
				WARNING("The XIA wrapper only supports AF_INET mappings.\n");
				errno = EAI_FAMILY;
				return -1;
			} else if (hints->ai_flags != 0) {
				WARNING("Flags to getaddrinfo are not currently implemented.\n");
			}
			socktype = hints->ai_socktype;
			protocol = hints->ai_protocol;
		}


		if (service) {
			port = strtol(service, NULL, 10);
			MSG("service:%d\n", port);
			if (errno == EINVAL) {
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

			if (_FailedHit(s)) {
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
//			ip2dag[s] = dag;
//			dag2ip[dag] = s;

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
			} else {
				MSG("family does not include XIA\n");
				tryxia = 0;
			}
		
		} else if (service) {
			MSG("service is: %s\n", service);
			// see if service looks like a DAG
			// FIXME: this should be made smarter
			if (strchr(service, ':') != NULL) {
				MSG("XIA service found\n");
				tryxia = 1;
			} else {
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

struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA

	return __real_gethostbyaddr(addr, len, type);
}

struct hostent *gethostbyname (const char *name)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA

	return __real_gethostbyname(name);
}

int gethostbyaddr_r (const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA

	return __real_gethostbyaddr_r(addr, len, type, result_buf, buf, buflen, result, h_errnop);
}

int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();

	// FIXME: add code here to map between IPv4 and XIA

	return __real_gethostbyname_r(name, result_buf, buf, buflen, result, h_errnop);
}

struct servent *getservbyname (const char *name, const char *proto)
{
	TRACE();
	ALERT();
	return __real_getservbyname(name, proto);
}

struct servent *getservbyport (int port, const char *proto)
{
	TRACE();
	ALERT();
	return __real_getservbyport(port, proto);
}

int getservbyname_r (const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	TRACE();
	ALERT();
	return __real_getservbyname_r(name, proto, result_buf, buf, buflen, result);
}

int getservbyport_r (int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	TRACE();
	ALERT();
	return __real_getservbyport_r(port, proto, result_buf, buf, buflen, result);
}

int getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags)
{
	TRACE();
	ALERT();
	return __real_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}
