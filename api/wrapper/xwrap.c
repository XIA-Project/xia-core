
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

/*
** FIXME:
** - are the setwrapped macros needed here anymore, or can they safely just be used in the xsocket api now?
** - gcc 4.7 as inlined some of the *printf functions, need to find new way to catch them
** - fclose doesn't capture correctly
** - implement getline
** - add support for getdelim (like getline)
** - fix fcntl to use va_list
** - don't know how to implement fcloseall
** - close will have problems when closing dup'd fds
** - dup functions need to mark the new fd somehow so rhe xsocket api knows it's special
** - reads/writes are atomic, can't merge multiple writes into a single read at the other end
** - fputs_unlocked/fgets_unlocked not defined for applications?
*/

/*
** See end of file for list of functions remapped and those that are ignored
*/

#define XIA_PRELOAD

#define TRACE()		{if (_log_trace)   __real_fprintf(_log, "xwrap: %s\r\n", __FUNCTION__);}
#define STOCK()		{if (_log_trace)   __real_fprintf(_log, "xwrap: %s informational tracing only\r\n", __FUNCTION__);}

#define MSG(...)	{if (_log_info)    __real_fprintf(_log, "xwrap: %s ", __FUNCTION__); __real_fprintf(_log, __VA_ARGS__);}

#define XIAIFY()	{if (_log_xia)     __real_fprintf(_log, "xwrap: %s redirected to XIA\r\n", __FUNCTION__);}
#define NOXIA()		{if (_log_xia)     __real_fprintf(_log, "xwrap: %s used normally\r\n", __FUNCTION__);}
#define SKIP()		{if (_log_xia)     __real_fprintf(_log, "xwrap: %s not required/supported in XIA (no-op)\r\n", __FUNCTION__);}

#define ALERT()		{if (_log_warning) __real_fprintf(_log, "xwrap: ALERT!!!, %s is not implemented in XIA!\r\n", __FUNCTION__);}
#define WARNING(...)	{if (_log_warning) __real_fprintf(_log, "xwrap: %s ", __FUNCTION__); __real_fprintf(_log, __VA_ARGS__);}

/*
** If XIA_PRELOAD is defined, the code below will be compiled as a library that
** can be loaded at runtime into an application giving it XIA support without
** requiring a recompile. Seed above for functions that require code changes.
**
# see the xia-core/bin/xwrap script for usage
*/
#ifdef XIA_PRELOAD

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
// Informationa remallings
//DECLARE(int, fileno, FILE *stream);
//DECLARE(int, fileno_unlocked, FILE *stream);

// need changes to click in order to work correctly
DECLARE(int, dup, int fd);
DECLARE(int, dup2, int fd, int fd2);
DECLARE(int, dup3, int fd, int fd2, int flags);

// not remapping correctly
//DECLARE(int, fclose, FILE *stream);

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
DECLARE(ssize_t, sendmsg, int fd, const struct msghdr *message, int flags);
DECLARE(ssize_t, recvmsg, int fd, struct msghdr *message, int flags);
DECLARE(int, getsockopt, int fd, int level, int optname, void *optval, socklen_t *optlen);
DECLARE(int, setsockopt, int fd, int level, int optname, const void *optval, socklen_t optlen);
DECLARE(int, listen, int fd, int n);
DECLARE(int, accept, int fd, struct sockaddr *addr, socklen_t *addr_len);
DECLARE(int, accept4, int fd, struct sockaddr *addr, socklen_t *addr_len, int flags);
DECLARE(int, shutdown, int fd, int how);
DECLARE(int, sockatmark, int fd);
DECLARE(int, close, int fd);
DECLARE(int, ioctl, int d, int request, ...);
DECLARE(int, fprintf, FILE *stream, const char *format, ...);
DECLARE(int, vfprintf, FILE *s, const char *format, va_list arg);
DECLARE(int, vdprintf, int fd, const char *fmt, va_list arg);
DECLARE(int, dprintf, int fd, const char *fmt, ...);
DECLARE(int, fgetc, FILE *stream);
DECLARE(int, getc, FILE *stream);
#ifdef __linux__
DECLARE(int, getc_unlocked, FILE *stream);
DECLARE(int, fgetc_unlocked, FILE *stream);
DECLARE(int, fputc_unlocked, int c, FILE *stream);
DECLARE(int, putc_unlocked, int c, FILE *stream);
DECLARE(char *, fgets_unlocked, char *s, int n, FILE *stream);
DECLARE(int, fputs_unlocked, const char *s, FILE *stream);
DECLARE(size_t, fread_unlocked, void *ptr, size_t size, size_t n, FILE *stream);
DECLARE(size_t, fwrite_unlocked, const void *ptr, size_t size, size_t n, FILE *stream);
#endif
DECLARE(int, fputc, int c, FILE *stream);
DECLARE(int, putc, int c, FILE *stream);
DECLARE(char *, fgets, char *s, int n, FILE *stream);
DECLARE(ssize_t, getline, char **lineptr, size_t *n, FILE *stream);
DECLARE(int, fputs, const char *s, FILE *stream);
DECLARE(int, puts, const char *s);
DECLARE(size_t, fread, void *ptr, size_t size, size_t n, FILE *stream);
DECLARE(size_t, fwrite, const void *ptr, size_t size, size_t n, FILE *s);
DECLARE(ssize_t, read, int fd, void *buf, size_t nbytes);
DECLARE(ssize_t, write, int fd, const void *buf, size_t n);
DECLARE(int, fcntl, int fd, int cmd, ...);
DECLARE(int, getaddrinfo, const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
DECLARE(void, freeaddrinfo, struct addrinfo *ai);
DECLARE(const char *, gai_strerror, int ecode);

DECLARE(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt);
DECLARE(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt);

// unsure what to do with
DECLARE(int, fcloseall, void);

// not remapping std* related calls for now
//DECLARE(int, printf, const char *format, ...);
//DECLARE(int, vprintf, const char *format, va_list arg);
//DECLARE(int, getchar, void);
//DECLARE(int, getchar_unlocked, void);
//DECLARE(int, putchar, int c);
//DECLARE(int, putchar_unlocked, int c);
//DECLARE(char *, gets, char *s);

// not ported to XIA, remapped for warning purposes
DECLARE(int, gethostname, char *name, size_t len);
DECLARE(int, sethostname, const char *name, size_t len);
#ifdef __APPLE__
DECLARE(void, sethostid, long int id);
#else
DECLARE(int, sethostid, long int id);
#endif
DECLARE(int, getdomainname, char *name, size_t len);
DECLARE(int, setdomainname, const char *name, size_t len);
DECLARE(long int, gethostid, void);
DECLARE(void, sethostent, int stay_open);
DECLARE(void, endhostent, void);
DECLARE(struct hostent *,gethostent, void);
DECLARE(struct hostent *,gethostbyaddr, const void *addr, socklen_t len, int type);
DECLARE(struct hostent *,gethostbyname, const char *name);
DECLARE(struct hostent *,gethostbyname2, const char *name, int af);
DECLARE(int, gethostent_r, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, gethostbyaddr_r, const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, gethostbyname_r, const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(int, gethostbyname2_r, const char *name, int af, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
DECLARE(void, setnetent, int stay_open);
DECLARE(void, endnetent, void);
DECLARE(struct netent *, getnetent, void);
DECLARE(struct netent *, getnetbyaddr, uint32_t net, int type);
DECLARE(struct netent *, getnetbyname, const char *name);
DECLARE(int, getnetent_r, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
DECLARE(int, getnetbyaddr_r, uint32_t net, int type, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
DECLARE(int, getnetbyname_r, const char *name, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
DECLARE(void, setservent, int stay_open);
DECLARE(void, endservent, void);
DECLARE(struct servent *, getservent, void);
DECLARE(struct servent *, getservbyname, const char *name, const char *proto);
DECLARE(struct servent *, getservbyport, int port, const char *proto);
DECLARE(int, getservent_r, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(int, getservbyname_r, const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(int, getservbyport_r, int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
DECLARE(void, setprotoent, int stay_open);
DECLARE(void, endprotoent, void);
DECLARE(struct protoent *, getprotoent, void);
DECLARE(struct protoent *, getprotobyname, const char *name);
DECLARE(struct protoent *, getprotobynumber, int proto);
DECLARE(int, getprotoent_r, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
DECLARE(int, getprotobyname_r, const char *name, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
DECLARE(int, getprotobynumber_r, int proto, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
#ifdef __APPLE__
DECLARE(void, setnetgrent, const char *netgroup);
#else
DECLARE(int, setnetgrent, const char *netgroup);
#endif
DECLARE(void, endnetgrent, void);
DECLARE(int, getnetgrent, char **hostp, char **userp, char **domainp);
DECLARE(int, innetgr, const char *netgroup, const char *host, const char *user, const char *domain);
DECLARE(int, getnetgrent_r, char **hostp, char **userp, char **domainp, char *buffer, size_t buflen);
DECLARE(int, getnameinfo, const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags);

// macros for putc and getc seem to already create these
//DECLARE(int, _IO_getc, _IO_FILE *__fp);
//DECLARE(int, _IO_putc, int __c, _IO_FILE *__fp);


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

	// must do this first so that they load before the trace macros are called
//	GET_FCN(fileno);
//	GET_FCN(fileno_unlocked);
//	GET_FCN(printf);
//	GET_FCN(vprintf);
	GET_FCN(fprintf);
	GET_FCN(vfprintf);

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
	GET_FCN(accept4);
	GET_FCN(shutdown);
	GET_FCN(sockatmark);
	GET_FCN(close);
	GET_FCN(ioctl);
	GET_FCN(fcloseall);
	GET_FCN(vdprintf);
	GET_FCN(dprintf);
	GET_FCN(fgetc);
	GET_FCN(getc);
//	GET_FCN(fclose);
//	GET_FCN(getchar);
#ifdef __linux__
	GET_FCN(getc_unlocked);
//	GET_FCN(getchar_unlocked);
	GET_FCN(fgetc_unlocked);
	GET_FCN(fputc_unlocked);
	GET_FCN(putc_unlocked);
	GET_FCN(fgets_unlocked);
	GET_FCN(fputs_unlocked);
	GET_FCN(fread_unlocked);
	GET_FCN(fwrite_unlocked);
//	GET_FCN(putchar_unlocked);
#endif 
	GET_FCN(fputc);
	GET_FCN(putc);
//	GET_FCN(putchar);
	GET_FCN(fgets);
//	GET_FCN(gets);
	GET_FCN(getline);
	GET_FCN(fputs);
	GET_FCN(puts);
	GET_FCN(fread);
	GET_FCN(fwrite);
	GET_FCN(read);
	GET_FCN(write);
	GET_FCN(gethostname);
	GET_FCN(sethostname);
	GET_FCN(sethostid);
	GET_FCN(getdomainname);
	GET_FCN(setdomainname);
	GET_FCN(gethostid);
	GET_FCN(fcntl);
	GET_FCN(sethostent);
	GET_FCN(endhostent);
	GET_FCN(gethostent);
	GET_FCN(gethostbyaddr);
	GET_FCN(gethostbyname);
	GET_FCN(gethostbyname2);
	GET_FCN(gethostent_r);
	GET_FCN(gethostbyaddr_r);
	GET_FCN(gethostbyname_r);
	GET_FCN(gethostbyname2_r);
	GET_FCN(setnetent);
	GET_FCN(endnetent);
	GET_FCN(getnetent);
	GET_FCN(getnetbyaddr);
	GET_FCN(getnetbyname);
	GET_FCN(getnetent_r);
	GET_FCN(getnetbyaddr_r);
	GET_FCN(getnetbyname_r);
	GET_FCN(setservent);
	GET_FCN(endservent);
	GET_FCN(getservent);
	GET_FCN(getservbyname);
	GET_FCN(getservbyport);
	GET_FCN(getservent_r);
	GET_FCN(getservbyname_r);
	GET_FCN(getservbyport_r);
	GET_FCN(setprotoent);
	GET_FCN(endprotoent);
	GET_FCN(getprotoent);
	GET_FCN(getprotobyname);
	GET_FCN(getprotobynumber);
	GET_FCN(getprotoent_r);
	GET_FCN(getprotobyname_r);
	GET_FCN(getprotobynumber_r);
	GET_FCN(setnetgrent);
	GET_FCN(endnetgrent);
	GET_FCN(getnetgrent);
	GET_FCN(innetgr);
	GET_FCN(getnetgrent_r);
	GET_FCN(getaddrinfo);
	GET_FCN(freeaddrinfo);
	GET_FCN(gai_strerror);
	GET_FCN(getnameinfo);
#if 0
	GET_FCN(_IO_getc);
	GET_FCN(_IO_putc);
#endif
	GET_FCN(dup);
	GET_FCN(dup2);
	GET_FCN(dup3);

	GET_FCN(readv);
	GET_FCN(writev);
}
#endif // XIA_PRELOAD


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


// called by the various <>printf wrappers
int _vasend(int fd, const char *format, va_list args)
{
//	va_list args;
//	va_start(args, format);

	char *s;
	int n = vasprintf(&s, format, args);

	markWrapped(fd);
	int rc = Xsend(fd, s, n, 0);
	markUnwrapped(fd);

	free(s);
//	va_end(args);

	return rc;
}

// get a single character from the socket, called by getc, getchar and associates
int _xgetc(int fd)
{
	char c;

	markWrapped(fd);
	int rc = Xrecv(fd, &c, 1, 0);
	markUnwrapped(fd);

	if (rc != -1)
		rc = c;
	return rc;
}

// put a single character to the socket, called by putc, putchar and associates
int _xputc(int i, int fd)
{
	unsigned char c = (unsigned char)i;

	markWrapped(fd);
	int rc = Xsend(fd, &c, 1, 0);
	markUnwrapped(fd);

	return rc;
}

int _xputs(const char *s, int fd)
{
	markWrapped(fd);
	int rc = Xsend(fd, s, strlen(s), 0);
	markUnwrapped(fd);

	return rc;
}


char *_xgets(int fd, char *s, int n)
{
	int i;
	int rc = -1;
	char *p = s;

	markWrapped(fd);
	for (i = 0; i < n - 1; i++) {
		rc = Xrecv(fd, p, 1, 0);
		p++;
		if (rc < 0 || *(p - 1) == '\n')
			break;
	}
	markUnwrapped(fd);

	if (rc < 0 && i == 0)
		s = NULL;
	else
		*p = '\0';

	return s;
}

int _xfread(void *buf, int size, int n, int fd)
{
	markWrapped(fd);
	int rc = Xrecv(fd, buf, size * n, 0);
	markUnwrapped(fd);

	if (rc >= 0)
		rc = rc / size;
	return rc;
}

int _xfwrite(const void *buf, int size, int n, int fd)
{
	markWrapped(fd);
	int rc = Xsend(fd, buf, size * n, 0);
	markUnwrapped(fd);

	if (rc >= 0)
		rc = rc / size;
	return rc;
}

int _xread(int fd, void *buf, size_t count)
{
	markWrapped(fd);
	int rc = Xrecv(fd, buf, count, 0);
	markUnwrapped(fd);

	return rc;
}

int _xwrite(int fd, const void *buf, size_t count)
{
	markWrapped(fd);
	int rc = Xsend(fd, buf, count, 0);
	markUnwrapped(fd);

	return rc;
}

/******************************************************************************
**
** FUNCTION REMAPPINGS START HERE
**
******************************************************************************/

#if 0
/*
** The following are informational remappings, all they do is print the
** parameter and return details. We can probab ly eliminate them when everything
** is working correctly.
*/
int fileno(FILE *stream)
{
//	TRACE();
	return __real_fileno(stream);
}
int fileno_unlocked(FILE *stream)
{
//	TRACE();
	return __real_fileno_unlocked(stream);
}
#endif
/*
** FIXME: dup,dup2,dup3 functionality won't be implemented until we have
** dup logic in click.
*/
int dup(int fd)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		errno = EINVAL;
		return -1;
	}

	return __real_dup(fd);
}

int dup2(int fd, int fd2)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		errno = EINVAL;
		return -1;
	}

	return __real_dup2(fd, fd2);
}

int dup3(int fd, int fd2, int flags)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		errno = EINVAL;
		return -1;
	}

	return __real_dup3(fd, fd2, flags);
}

/*
 File I/O remappings start here
*/
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

int accept4(int fd, struct sockaddr *addr, socklen_t *addr_len, int flags)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		markWrapped(fd);
		rc = Xaccept4(fd, addr, addr_len, flags);
		markUnwrapped(fd);

	} else {
		rc = __real_accept4(fd, addr, addr_len, flags);
	}
	return rc;
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	int rc;
	TRACE();

	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
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
//	if (isXsocket(fd) {
		XIAIFY();
		// FIXME: need to add async support
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

//	if (0) {
	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
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

//	if (0) {
	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
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

int shutdown(int fd, int how)
{
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		return __real_shutdown(fd, how);
	}
}

int sockatmark(int fd)
{
	/* NOTE: XIA has no concept of out of band data in the current version
	** this function will always return 0 for now
	*/
	TRACE();
	if (isXsocket(fd)) {
		ALERT();
		return 0;

	} else {
		return __real_sockatmark(fd);
	}
}

//#if 0

/* These are inlined as of GCC 4.7, so we can't intercept them anymore
** need to document them instead as not working. However, they seem less
** likely to be called directly on a socket than some of the other *printf
** functions
*/
int vfprintf(FILE *s, const char *format, va_list arg)
{
	int fd = fileno(s);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _vasend(fd, format, arg);

	} else {
		rc = __real_vfprintf(s, format, arg);
	}
	return rc;
}

int vdprintf(int fd, const char *fmt, va_list arg)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _vasend(fd, fmt, arg);

	} else {
		NOXIA();
		rc = __real_vdprintf(fd, fmt, arg);
	}
	return rc;
}

#if 0
// FIXME: this is probably not necessary. Anyone who remaps std* deserves what they get
int vprintf(const char *format, va_list arg)
{
	int fd = fileno(stdout);
	int rc;

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _vasend(fd, format, arg);

	} else {
		rc = __real_vprintf(format, arg);
	}
	return rc;
}

// FIXME: this is probably not necessary. Anyone who remaps std* deserves what they get
int printf(const char *format, ...)
{
	int rc;
	va_list args;
	va_start(args, format);

	rc = vprintf(format, args);
	va_end(args);
	return rc;
}
#endif

int fprintf(FILE *stream, const char *format, ...)
{
	int rc;
	va_list args;
	va_start(args, format);

	TRACE();
	rc = vfprintf(stream, format, args);
	va_end(args);
	return rc;
}

int dprintf(int fd, const char *fmt, ...)
{
	int rc;
	va_list args;
	va_start(args, fmt);

	TRACE();
	rc = vdprintf(fd, fmt, args);
	va_end(args);
	return rc;
}
//#endif

int fgetc(FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_fgetc(stream);
	}
	return rc;
}

// NOTE: this seems to be getting macro replaced onto IO_getc
int getc(FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getc(stream);
	}
	return rc;
}

#if 0
// FIXME: these are probably not necessary. Anyone who remaps std* to a socket deserves what they get
int getchar(void)
{
	int fd = fileno(stdin);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getc(stdin);
	}
	return rc;
}

int getchar_unlocked(void)
{
	int fd = fileno(stdin);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getchar_unlocked();
	}
	return rc;
}
#endif

#ifdef __linux__
int getc_unlocked(FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getc_unlocked(stream);
	}
	return rc;
}

int fgetc_unlocked(FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_fgetc(stream);
	}
	return rc;
}

int fputc_unlocked(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_fputc_unlocked(c, stream);
	}
	return rc;
}

int putc_unlocked(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putc_unlocked(c, stream);
	}
	return rc;
}

char *fgets_unlocked(char *s, int n, FILE *stream)
{
	int fd = fileno(stream);
	char *rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, n);

	} else {
		rc = __real_fgets_unlocked(s, n, stream);
	}
	return rc;
}

int fputs_unlocked(const char *s, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputs(s, fd);

	} else {
		rc = __real_fputs_unlocked(s, stream);
	}
	return rc;
}

size_t fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream)
{
	int fd = fileno(stream);
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfread(ptr, size, n, fd);

	} else {
		rc = __real_fread_unlocked(ptr, size, n, stream);
	}
	return rc;
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t n, FILE *stream)
{
	int fd = fileno(stream);
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfwrite(ptr, size, n, fd);

	} else {
		rc = __real_fwrite_unlocked(ptr, size, n, stream);
	}
	return rc;
}
#endif

int fputc(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_fputc(c, stream);
	}
	return rc;
}

int putc(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putc(c, stream);
	}
	return rc;
}

#if 0
// FIXME: these are probably not necessary. Anyone who remaps std* to a socket deserves what they get
int putchar(int c)
{
	int fd = fileno(stdout);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putchar(c);
	}
	return rc;
}

int putchar_unlocked(int c)
{
	int fd = fileno(stdout);
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putchar_unlocked(c);
	}
	return rc;
}
#endif

char *fgets(char *s, int n, FILE *stream)
{
	int fd = fileno(stream);
	char *rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, n);

	} else {
		rc = __real_fgets(s, n, stream);
	}
	return rc;
}

#if 0
// FIXME: these are probably not necessary. Anyone who remaps std* to a socket deserves what they get
char *gets(char *s)
{
	int fd = fileno(stdin);
	char *rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, INT_MAX);

	} else {
		rc = __real_gets(s);
	}
	return rc;
}
#endif

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	int fd = fileno(stream);
	ssize_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		// FIXME: implement the code here
		rc = 0;
	} else {
		NOXIA();
		rc = __real_getline(lineptr, n, stream);
	}
	return rc;
}

int fputs(const char *s, FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputs(s, fd);

	} else {
		rc = __real_fputs(s, stream);
	}
	return rc;
}

int puts(const char *s)
{
	int fd = fileno(stdout);
	int rc;

	//TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputs(s, fd);

	} else {
		rc = __real_puts(s);
	}
	return rc;
}

size_t fread(void *ptr, size_t size, size_t n, FILE *stream)
{
	int fd = fileno(stream);
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfread(ptr, size, n, fd);

	} else {
		rc = __real_fread(ptr, size, n, stream);
	}
	return rc;
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *s)
{
	int fd = fileno(s);
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfwrite(ptr, size, n, fd);

	} else {
		rc = __real_fwrite(ptr, size, n, s);
	}
	return rc;
}



ssize_t read(int fd, void *buf, size_t nbytes)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xread(fd, buf, nbytes);

	} else {
		rc = __real_read(fd, buf, nbytes);
	}
	return rc;
}

ssize_t write(int fd, const void *buf, size_t n)
{
	size_t rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xwrite(fd, buf, n);

	} else {
		rc = __real_write(fd, buf, n);
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
			WARNING("not enough room for the XIA sockaddr! have %d, need %d\n", *len, sizeof(sockaddr_x));
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
			WARNING("not enough room for the XIA sockaddr! have %d, need %d\n", *len, sizeof(sockaddr_x));
		markWrapped(fd);
		rc = Xgetpeername(fd, addr, len);
		markUnwrapped(fd);

	} else {
		return __real_getpeername(fd, addr, len);
	}
	return rc;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		int i, size = 0;
		char *p, *buf;
		
		XIAIFY();

		// readv is atomic, so make one buffer big enough to handle
		// the requsted size, and then use it to fill the iovecs
		for (i = 0; i < iovcnt; i++)
			size += iov[i].iov_len;

		p = buf = (char *)malloc(size);
		rc = _xread(fd, buf, size);

		if (rc >= 0) {
			for (i = 0; i < iovcnt; i++) {
				if (size <= 0)
					break;
				int cnt = MIN(size, iov[i].iov_len);

				memcpy(iov[i].iov_base, p, cnt);
				p += cnt;
				size -= cnt;
			}
		}

		free(buf);

	} else {
		rc = __real_readv(fd, iov, iovcnt);
	}
	return rc;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	int rc;

	TRACE();
	if (isXsocket(fd)) {
		int i, size = 0;
		char *p, *buf;
		
		XIAIFY();

		// writev is atomic, so put everything into a buffer instead of
		// writing out each individual iovec
		for (i = 0; i < iovcnt; i++)
			size += iov[i].iov_len;

		p = buf = (char *)malloc(size);
	
		for (i = 0; i < iovcnt; i++) {
			memcpy(p, iov[i].iov_base, iov[i].iov_len);
			p += iov[i].iov_len;
		}
	
		rc = _xwrite(fd, buf, size);
		free(buf);
	} else {
		rc = __real_writev(fd, iov, iovcnt);
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
#if 0
int fclose (FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	TRACE();

	if (isXsocket(fd)) {
		XIAIFY();
		// fIXME: need to close the XIA socket, but still let this do what
		// it needs to
		// maybe dup the socket and close that?
		rc = -1;
	} else {
		printf("real fclose = %p", __real_fclose);
		NOXIA();
		MSG("real fclose = %p", __real_fclose);
		rc = __real_fclose(stream);
	}

	return rc;
}
#endif

int fcloseall (void)
{
	TRACE();

	MSG("not sure what to do with this function");
	// FIXME: what do we do with this??

	return __real_fcloseall();
}

int gethostname (char *name, size_t len)
{
	int rc;

	TRACE();
	ALERT();

	rc = __real_gethostname(name, len);
	return rc;
}

int sethostname (const char *name, size_t len)
{
	int rc;

	TRACE();
	ALERT();

	rc = __real_sethostname(name, len);
	return rc;
}

#ifdef __APPLE__
void sethostid (long int id)
{
	TRACE();
	ALERT();

	__real_sethostid(id);
}
#else
int sethostid (long int id)
{
	int rc;

	TRACE();
	ALERT();

	rc = __real_sethostid(id);
	return rc;
}
#endif

int getdomainname (char *name, size_t len)
{
	int rc;

	TRACE();
	ALERT();

	rc = __real_getdomainname(name, len);
	return rc;
}

int setdomainname (const char *name, size_t len)
{
	int rc;

	TRACE();
	ALERT();

	rc = __real_setdomainname(name, len);
	return rc;
}

long int gethostid (void)
{
	TRACE();
	ALERT();

	return __real_gethostid();
}

void sethostent (int stay_open)
{
	TRACE();
	ALERT();

	__real_sethostent(stay_open);
}

void endhostent (void)
{
	TRACE();
	ALERT();

	__real_endhostent();
}

struct hostent *gethostent (void)
{
	TRACE();
	ALERT();
	return __real_gethostent();
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

struct hostent *gethostbyname2 (const char *name, int af)
{
	TRACE();
	ALERT();
	return __real_gethostbyname2(name, af);
}

int gethostent_r (struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_gethostent_r(result_buf, buf, buflen, result, h_errnop);
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

int gethostbyname2_r (const char *name, int af, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_gethostbyname2_r(name, af, result_buf, buf, buflen, result, h_errnop);
}

void setnetent (int stay_open)
{
	TRACE();
	ALERT();
	return __real_setnetent(stay_open);
}

void endnetent (void)
{
	TRACE();
	ALERT();
	return __real_endnetent();
}

struct netent *getnetent (void)
{
	TRACE();
	ALERT();
	return __real_getnetent();
}

struct netent *getnetbyaddr (uint32_t net, int type)
{
	TRACE();
	ALERT();
	return __real_getnetbyaddr(net, type);
}

struct netent *getnetbyname (const char *name)
{
	TRACE();
	ALERT();
	return __real_getnetbyname(name);
}

int getnetent_r (struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_getnetent_r(result_buf, buf, buflen, result, h_errnop);
}

int getnetbyaddr_r (uint32_t net, int type, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_getnetbyaddr_r(net, type, result_buf, buf, buflen, result, h_errnop);
}

int getnetbyname_r (const char *name, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	TRACE();
	ALERT();
	return __real_getnetbyname_r(name, result_buf, buf, buflen, result, h_errnop);
}

void setservent (int stay_open)
{
	TRACE();
	ALERT();
	__real_setservent(stay_open);
}

void endservent (void)
{
	TRACE();
	ALERT();
	__real_endservent();
}


struct servent *getservent (void)
{
	TRACE();
	ALERT();
	return __real_getservent();
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

int getservent_r (struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	TRACE();
	ALERT();
	return __real_getservent_r(result_buf, buf, buflen, result);
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

void setprotoent (int stay_open)
{
	TRACE();
	ALERT();
	__real_setprotoent(stay_open);
}

void endprotoent (void)
{
	TRACE();
	ALERT();
	__real_endprotoent();
}

struct protoent *getprotoent (void)
{
	TRACE();
	ALERT();
	return __real_getprotoent();
}

struct protoent *getprotobyname (const char *name)
{
	TRACE();
	ALERT();
	return __real_getprotobyname(name);
}

struct protoent *getprotobynumber (int proto)
{
	TRACE();
	ALERT();
	return __real_getprotobynumber(proto);
}

int getprotoent_r (struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	TRACE();
	ALERT();
	return __real_getprotoent_r(result_buf, buf, buflen, result);
}

int getprotobyname_r (const char *name, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	TRACE();
	ALERT();
	return __real_getprotobyname_r(name, result_buf, buf, buflen, result);
}

int getprotobynumber_r (int proto, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	TRACE();
	ALERT();
	return __real_getprotobynumber_r(proto, result_buf, buf, buflen, result);
}

#ifdef __APPLE__
void setnetgrent (const char *netgroup)
{
	TRACE();
	ALERT();
	__real_setnetgrent(netgroup);
}
#else
int setnetgrent (const char *netgroup)
{
	TRACE();
	ALERT();
	return __real_setnetgrent(netgroup);
}
#endif

void endnetgrent (void)
{
	TRACE();
	ALERT();
	__real_endnetgrent();
}

int getnetgrent (char **hostp, char **userp, char **domainp)
{
	TRACE();
	ALERT();
	return __real_getnetgrent(hostp, userp, domainp);
}

int innetgr (const char *netgroup, const char *host, const char *user, const char *domain)
{
	TRACE();
	ALERT();
	return __real_innetgr(netgroup, host, user, domain);
}

int getnetgrent_r (char **hostp, char **userp, char **domainp, char *buffer, size_t buflen)
{
	TRACE();
	ALERT();
	return __real_getnetgrent_r(hostp, userp, domainp, buffer, buflen);
}

int getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags)
{
	TRACE();
	ALERT();
	return __real_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

#if 0
/*
** these seem to be handled by macros for putc and getc
*/
int _IO_getc (_IO_FILE *__fp)
{
	size_t rc;

	TRACE();

	if (isXsocket(fp->fileno)) {
		XIAIFY();
		// FIXME: implement the code here
		rc = -1;
	} else {
		NOXIA();
		rc = __real__IO_getc(fp);
	}
	return rc;
}

int _IO_putc (int __c, _IO_FILE *__fp)
{
	size_t rc;

	TRACE();

	if (isXsocket(fp->fileno)) {
		XIAIFY();
		// FIXME: implement the code here
		rc = -1;
	} else {
		NOXIA();
		rc = __real__IO_putc(c, fp);
	}
	return rc;
}
#endif

/*
** Not remapped functions because they shoudn't be used with sockets
** int fflush (FILE *stream);
** int fflush_unlocked (FILE *stream);
** FILE *fopen (const char *filename, const char *modes);
** FILE *freopen (const char *filename, const char *modes, FILE *stream);
** FILE *fopen64 (const char *filename, const char *modes);
** FILE *freopen64 (const char *filename, const char *modes, FILE *stream);
** FILE *fdopen (int fd, const char *modes);
** void setbuf (FILE *stream, char *buf);
** int setvbuf (FILE *stream, char *buf, int modes, size_t n);
** void setbuffer (FILE *stream, char *buf, size_t size);
** void setlinebuf (FILE *stream);
** int fseek (FILE *stream, long int off, int whence);
** long int ftell (FILE *stream);
** void rewind (FILE *stream);
** int fseeko (FILE *stream, off_t off, int whence);
** off_t ftello (FILE *stream);
** int fgetpos (FILE *stream, fpos_t *pos);
** int fsetpos (FILE *stream, const fpos_t *pos);
** int fseeko64 (FILE *stream, off64_t off, int whence);
** int fgetpos64 (FILE *stream, fpos64_t *pos);
** int fsetpos64 (FILE *stream, const fpos64_t *pos);
** void flockfile (FILE *stream);
** int ftrylockfile (FILE *stream);
** void funlockfile (FILE *stream);
** ssize_t pread (int fd, void *buf, size_t nbytes, off_t offset);
** ssize_t pwrite (int fd, const void *buf, size_t n, off_t offset);
** ssize_t pread64 (int fd, void *buf, size_t nbytes, off64_t offset);
** ssize_t pwrite64 (int fd, const void *buf, size_t n, off64_t offset);
** int open (const char *file, int oflag, ...);
** int open64 (const char *file, int oflag, ...);
** int openat (int fd, const char *file, int oflag, ...)
** int creat (const char *file, mode_t mode);
** int creat64 (const char *file, mode_t mode);
** int preadv();
** int pwritev();
**
** These functions probably won't be used in socket calls
** int fscanf (FILE *stream, const char *format, ...);
** int isoc99_fscanf (FILE *stream, const char *format, ...);
** int vfscanf (FILE *s, const char *format, _G_va_list arg)
** int vscanf (const char *format, _G_va_list arg);
** int isoc99_vfscanf (FILE *s, const char *format, _G_va_list arg);
** int getw (FILE *stream);
** int putw (int w, FILE *stream);
** int ungetc (int c, FILE *stream);
** int feof (FILE *stream);
** int ferror (FILE *stream);
** void clearerr_unlocked (FILE *stream);
** int feof_unlocked (FILE *stream);
** int ferror_unlocked (FILE *stream);
** int getaddrinfo_a (int mode, struct gaicb *list[restrict_arr], int ent, struct sigevent *sig);
** int gai_suspend (const struct gaicb *const list[], int ent, const struct timespec *timeout);
** int gai_error (struct gaicb *req);
** int gai_cancel (struct gaicb *gaicbp);
** int _IO_feof (_IO_FILE *__fp);
** int _IO_ferror (_IO_FILE *__fp) __THROW;
** int _IO_peekc_locked (_IO_FILE *__fp);
*/
