
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

#define TRACE()		{if (_log_trace)   fprintf(_log, "xwrap: %s\r\n", __FUNCTION__);}
#define STOCK()		{if (_log_trace)   fprintf(_log, "xwrap: %s informational tracing only\r\n", __FUNCTION__);}

#define MSG(...)	{if (_log_info)    fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}

#define XIAIFY()	{if (_log_xia)     fprintf(_log, "xwrap: %s redirected to XIA\r\n", __FUNCTION__);}
#define NOXIA()		{if (_log_xia)     fprintf(_log, "xwrap: %s used normally\r\n", __FUNCTION__);}
#define SKIP()		{if (_log_xia)     fprintf(_log, "xwrap: %s not required/supported in XIA (no-op)\r\n", __FUNCTION__);}

#define ALERT()		{if (_log_warning) fprintf(_log, "xwrap: ALERT!!!, %s is not implemented in XIA!\r\n", __FUNCTION__);}
#define WARNING(...)	{if (_log_warning) fprintf(_log, "xwrap: %s ", __FUNCTION__); fprintf(_log, __VA_ARGS__);}

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

//****************************************************************************
// set up logging parameters
//
int _log_trace = 0;
int _log_warning = 0;
int _log_info = 0;
int _log_xia = 0;
FILE *_log = NULL;

void __xwrap_setup()
{
	if (getenv("XWRAP_TRACE") != NULL)
		_log_trace = 1;
	if (getenv("XWRAP_VERBOSE") != NULL)
		_log_trace = _log_info = _log_xia = _log_warning = 1;
	if (getenv("XWRAP_INFO") != NULL)
		_log_info = 1;
	if (getenv("XWRAP_WARNING") != NULL)
		_log_warning = 1;
	if (getenv("XWRAP_XIA") != NULL)
		_log_xia = 1;

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

	if (_log_info || _log_warning || _log_xia || _log_trace)
		fprintf(_log, "loading XIA wrappers (created: %s)\n", __DATE__);

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
#define isXsocket(s)	 (getSocketType(s) != -1)
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

/******************************************************************************
**
** FUNCTION REMAPPINGS START HERE
**
******************************************************************************/

int socket(int domain, int type, int protocol)
{
	int fd;
	TRACE();

	if (domain == AF_XIA) {
		XIAIFY();
		fd = Xsocket(domain, type, protocol);
	} else {
		fd = __real_socket(domain, type, protocol);
	}
	return fd;
}

int socketpair(int domain, int type, int protocol, int fds[2])
{
	TRACE();
	if (domain == AF_XIA) {
		XIAIFY();
		fds[0] = Xsocket(domain, type, protocol);
		fds[1] = Xsocket(domain, type, protocol);

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

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
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

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
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

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xaccept(fd, addr, addr_len);
		markUnwrapped(fd);

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

	if (shouldWrap(fd)) {
		XIAIFY();
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
	TRACE();

	if (shouldWrap(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xrecvfrom(fd, buf, n, flags, addr, addr_len);
		markUnwrapped(fd);
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

	} else {
		rc = __real_close(fd);
	}
	return rc;
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		if (*len < sizeof(sockaddr_x)) {
			WARNING("not enough room for the XIA sockaddr! have %d, need %ld\n", *len, sizeof(sockaddr_x));
		}
		markWrapped(fd);
		rc = Xgetsockname(fd, addr, len);
		markUnwrapped(fd);

	} else {
		rc = __real_getsockname(fd, addr, len);
	}
	return rc;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		if (*len < sizeof(sockaddr_x))
			WARNING("not enough room for the XIA sockaddr! have %d, need %ld\n", *len, sizeof(sockaddr_x));
		markWrapped(fd);
		rc = Xgetpeername(fd, addr, len);
		markUnwrapped(fd);

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

int getaddrinfo (const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai)
{
	int rc = -1;
	int tryxia = 1;
	int trynormal = 1;

	TRACE();
	// try to determine if this is an XIA address lookup
	if (req) {
		// they specified hints, see if the family is XIA, or unspecified
		if (req->ai_family == AF_UNSPEC) {
			MSG("family is unspec\n");
			tryxia = 1;
		} else if (req->ai_family == AF_XIA) {
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
		rc = Xgetaddrinfo(name, service, req, pai);
		MSG("Xgetaddrinfo returns %d\n", rc);
	}

	// XIA lookup failed, fall back to 
	if (trynormal && rc < 0) {	
		NOXIA();
		MSG("looking up name normally\n");
		rc = __real_getaddrinfo(name, service, req, pai);
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
	return __real_gethostbyaddr(addr, len, type);
}

struct hostent *gethostbyname (const char *name)
{
	TRACE();
	ALERT();
	return __real_gethostbyname(name);
}

int gethostbyaddr_r (const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_gethostbyaddr_r(addr, len, type, result_buf, buf, buflen, result, h_errnop);
}

int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();
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

