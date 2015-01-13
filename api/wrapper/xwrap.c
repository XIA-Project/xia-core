
//#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
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

ip2dag_t ip2dag;
dag2ip_t dag2ip;

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
	_wrap_socket = 1;

	if (getenv("XWRAP_XIA") != NULL)
		_pure_xia = 1;

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
}


/******************************************************************************
**
** Helper functions
**
******************************************************************************/

// call into the Xsockets API to see if the fd is associated with an Xsocket
#define isXsocket(s)	 (_pure_xia == 1 || getSocketType(s) != -1)
#define markWrapped(s)	 (setWrapped(s, 1))
#define markUnwrapped(s) (setWrapped(s, 0))
#define shouldWrap(s)	 (isXsocket(s) && !isWrapped(s))

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
int _NewIP(sockaddr_x *sax, struct sockaddr_in *sin, int port)
{
	char s[64];
	char s1[64];

	if (port == 0)
		port = _NewPort();

	sprintf(s, "169.254.%d.%d", high_ip, low_ip);
	inet_aton(s, &sin->sin_addr);

	sin->sin_family = AF_INET;
	sin->sin_port = port;

	sprintf(s1, "%s-%d", s, ntohs(sin->sin_port));
printf("nip %s\n", s);
printf("new ip %s\n", s1);

	Graph g(sax);

	std::string dag = g.dag_string();

	ip2dag[s1] = dag;
	dag2ip[dag] = s1;

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
	snprintf(buf, len, "SID:44444ff0000000000000000000000000%08x", rand());

	return buf;
}

// convert a IPv4 sock addr into a sting in the form of A.B.C.D-port
char *_IpString(char *s, struct sockaddr_in* sa)
{
	// FIXME: this doesn't seem threadsafe
	char *ip = inet_ntoa(sa->sin_addr);

	// make a name from the ip address and port
	sprintf(s, "%s-%u", ip, ntohs(sa->sin_port));
	printf("%s\n", s);
	return s;
}

// map from IP to XIA
int _i2x(struct sockaddr_in *sin, sockaddr_x *sax)
{
	char s[64];
	std::string dag = ip2dag[_IpString(s, sin)];

	printf("dag=%s\n", dag.c_str());

	Graph g(dag);
	g.fill_sockaddr(sax);

	// FIXME: do error checking here
	// return -1 if the lookup fails

	return 0;
}

// map from XIA to IP
int _x2i(sockaddr_x *sax, sockaddr_in *sin)
{
	// FIXME: this depends on the created dag string always looking the same!
	char name[64];
	Graph g(sax);
	strcpy(name, dag2ip[g.dag_string()].c_str());

	// chop name into ip address and port
	char *p = strchr(name, '-');
	*p++ = 0;

	inet_aton(name, &sin->sin_addr);
	sin->sin_port = atoi(p);
	sin->sin_family = AF_INET;

	// FIXME: do error checking here
	// return -1 if the lookup fails
	
	return 0;	
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

/******************************************************************************
**
** FUNCTION REMAPPINGS START HERE
**
******************************************************************************/

int socket(int domain, int type, int protocol)
{
	int fd;
	TRACE();

printf("t:%d p:%d %d %d\n", type, protocol, SOCK_STREAM, SOCK_DGRAM);
	if ((domain == AF_XIA || FORCE_XIA()) && _wrap_socket) {
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
	if (domain == AF_XIA || FORCE_XIA()) {
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
	if (isXsocket(fd)) {
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

	printf("accept:%d\n", fd);
	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		xlen = sizeof(sax);
		markWrapped(fd);
		rc = Xaccept(fd, addr, &xlen);
		markUnwrapped(fd);
	printf("accept new:%d\n", rc);

		if (FORCE_XIA()) {
			// create a new fake IP address/port  to map to
			_NewIP(&sax, (struct sockaddr_in*)ipaddr, 0);

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
	printf("recv:%d\n", fd);
	if (shouldWrap(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xrecv(fd, buf, n, flags);
		markUnwrapped(fd);
	} else {
		rc = __real_recv(fd, buf, n, flags);
	}
	return rc;
}

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
{
	int rc;
	TRACE();
	sockaddr_x sax;


	if (shouldWrap(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			// FIXME: this will currently fail if the application creates the sockaddr
			// internally instead of getting it from getaddrinfo, we probably need
			// to add a name lookup if the mapping lookup fails

			// create a mapping from IP/port to a dag and register it
			_Register(addr, addr_len);

			// convert the sockaddr to a sockaddr_x
			_i2x((struct sockaddr_in*)addr, &sax);
			addr = (struct sockaddr*)&sax;
		}

		markWrapped(fd);
		rc = Xsendto(fd, buf, n, flags, addr, addr_len);
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

	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();

		if (FORCE_XIA()) {
			ipaddr = addr;
			addr = (struct sockaddr*)&sax;
		}

		markWrapped(fd);
		rc = Xrecvfrom(fd, buf, n, flags, addr, addr_len);
		markUnwrapped(fd);

		if (FORCE_XIA()) {
			// convert the sockaddr to a sockaddr_x
			if (_x2i(&sax, (struct sockaddr_in*)ipaddr) < 0) {
				// we don't have a mapping for this yet, create a fake IP address
				_NewIP(&sax, (sockaddr_in *)ipaddr, 0);
			}
		}	

	} else {
		rc = __real_recvfrom(fd, buf, n, flags, addr, addr_len);
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
	if (isXsocket(fd)) {
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
				_NewIP(&sax, (sockaddr_in*)ipaddr, 0);
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

	if (FORCE_XIA()) {
		sockaddr_x sax;
		char s[64];
		int socktype = 0;
		int protocol = 0;
		int family = AF_INET;
		int flags = 0;
		int port;
		socklen_t len;


		printf("getaddrinfo:\n");
		printf("  flags: %08x\n", hints->ai_flags);

		// FIXME: this assumes that name is an IP string
		//  instead of a name

		if (hints) {
			if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET) {
				printf("The XIA wrapper only supports AF_INET mappings.\n");
				errno = EAI_FAMILY;
				return -1;
			} else if (hints->ai_flags != 0) {
				printf("Flags to getaddrinfo are not currently implemented.\n");
			}
			socktype = hints->ai_socktype;
			protocol = hints->ai_protocol;
		}

		if (service) {
			port = strtol(service, NULL, 10);
			if (errno) {
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

		sprintf(s, "%s-%d", name, port);
		printf("%s\n", s);
		if (XgetDAGbyName(s, &sax, &len) < 0) {
			printf("name lookup error\n");
			errno = EAI_NONAME;
			return -1;
		}

		struct addrinfo *ai = (struct addrinfo *)calloc(sizeof(struct addrinfo), 1);

		// fill in the blanks
		ai->ai_family    = family;
		ai->ai_socktype  = socktype;
		ai->ai_protocol  = protocol;
		ai->ai_flags     = flags;
		ai->ai_addrlen   = sizeof(struct sockaddr);
		ai->ai_next      = NULL;

		ai->ai_addr = (struct sockaddr *)calloc(sizeof(struct sockaddr), 1);

		_NewIP(&sax, (sockaddr_in *)ai->ai_addr, htons(port));
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

