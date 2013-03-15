
//#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

/*
** FIXME:
** - fclose doesn't capture correctly
** - socket and socketpair take flags for async and close on exec. need to handle them
** - need to implement async sockets
** - what should we do with unsupport socket options in get/setsockopt? 
** - implement getline
** - add support for getdelim (like getline)
** - properly implement gets
** - figure out test for stdin/stdout cases
** - close will have problems when closing dup'd fds
** - dup functions need to mark the new fd somehow so rhe xsocket api knows it's special
** - reads/writes are atomic, can't merge multiple writes into a single read at the other end
** - fputs_unlocked/fgets_unlocked not defined for applications?
*/

/* 
** See end of file for list of functions remapped and those that are ignored
*/

#define DEBUG
#define TRACE_ALL
#define TRACE_XIA
#define XIA_PRELOAD

#ifdef TRACE_ALL
#define FLAG()		__real_printf("%s: called\r\n", __FUNCTION__)
#else
#define FLAG()
#endif

#ifdef TRACE_XIA
#define XIAIFY()	__real_printf("%s: CATCHING XIA SOCKET\r\n", __FUNCTION__)
#else
#define XIAIFY()
#endif

#ifdef DEBUG
#define MSG(...)	__real_printf(__VA_ARGS__)
#define TRACE(...)	__real_printf("%s: ", __FUNCTION__); __real_printf(__VA_ARGS__)
#define NOXIA()		__real_printf("%s: used normally\r\n", __FUNCTION__)
#define SKIP()		__real_printf("%s: not required/supported in XIA (no-op)\r\n", __FUNCTION__)
#define STOCK()		__real_printf("%s: tracing only\r\n", __FUNCTION__)
#define ALERT()		__real_printf("ALERT!!!, %s needs wrapping!\r\n", __FUNCTION__)
#else
#define MSG(...)
#define TRACE(...)
#define NOXIA()
#define SKIP()
#define STOCK()
#endif // DEBUG

/*
** This code can be added to any socket based application either at compile time 
** or runtime depending on how it is built.
*/
/*
** If XIA_PRELOAD is defined, the code below will be compiled as a library that
** can be loaded at runtime into an application giving it XIA support without
** requiring a recompile. Seed above for functions that require code changes.
**
** To use in the preload situation:
** LD_PRELOAD="xwrap.so libXsocket.so" application
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
DECLARE(int, dup, int fd);
DECLARE(int, dup2, int fd, int fd2);
DECLARE(int, dup3, int fd, int fd2, int flags);
DECLARE(int, fileno, FILE *stream);
DECLARE(int, fileno_unlocked, FILE *stream);
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
//DECLARE(int, fclose, FILE *stream);
DECLARE(int, fcloseall, void);
DECLARE(int, fprintf, FILE *stream, const char *format, ...);
DECLARE(int, printf, const char *format, ...);
DECLARE(int, vfprintf, FILE *s, const char *format, va_list arg);
DECLARE(int, vprintf, const char *format, va_list arg);
DECLARE(int, vdprintf, int fd, const char *fmt, va_list arg);
DECLARE(int, dprintf, int fd, const char *fmt, ...);
DECLARE(int, fgetc, FILE *stream);
DECLARE(int, getc, FILE *stream);
DECLARE(int, getchar, void);
DECLARE(int, getc_unlocked, FILE *stream);
DECLARE(int, getchar_unlocked, void);
DECLARE(int, fgetc_unlocked, FILE *stream);
DECLARE(int, fputc, int c, FILE *stream);
DECLARE(int, putc, int c, FILE *stream);
DECLARE(int, putchar, int c);
DECLARE(int, fputc_unlocked, int c, FILE *stream);
DECLARE(int, putc_unlocked, int c, FILE *stream);
DECLARE(int, putchar_unlocked, int c);
DECLARE(char *, fgets, char *s, int n, FILE *stream);
DECLARE(char *, gets, char *s);
DECLARE(char *, fgets_unlocked, char *s, int n, FILE *stream);
DECLARE(_IO_ssize_t, getline, char **lineptr, size_t *n, FILE *stream);
DECLARE(int, fputs, const char *s, FILE *stream);
DECLARE(int, puts, const char *s);
DECLARE(size_t, fread, void *ptr, size_t size, size_t n, FILE *stream);
DECLARE(size_t, fwrite, const void *ptr, size_t size, size_t n, FILE *s);
DECLARE(int, fputs_unlocked, const char *s, FILE *stream);
DECLARE(size_t, fread_unlocked, void *ptr, size_t size, size_t n, FILE *stream);
DECLARE(size_t, fwrite_unlocked, const void *ptr, size_t size, size_t n, FILE *stream);
DECLARE(ssize_t, read, int fd, void *buf, size_t nbytes);
DECLARE(ssize_t, write, int fd, const void *buf, size_t n);
DECLARE(int, gethostname, char *name, size_t len);
DECLARE(int, sethostname, const char *name, size_t len);
DECLARE(int, sethostid, long int id);
DECLARE(int, getdomainname, char *name, size_t len);
DECLARE(int, setdomainname, const char *name, size_t len);
DECLARE(long int, gethostid, void);
DECLARE(int, fcntl, int fd, int cmd, ...);
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
DECLARE(int, setnetgrent, const char *netgroup);
DECLARE(void, endnetgrent, void);
DECLARE(int, getnetgrent, char **hostp, char **userp, char **domainp);
DECLARE(int, innetgr, const char *netgroup, const char *host, const char *user, const char *domain);
DECLARE(int, getnetgrent_r, char **hostp, char **userp, char **domainp, char *buffer, size_t buflen);
DECLARE(int, getaddrinfo, const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
DECLARE(void, freeaddrinfo, struct addrinfo *ai);
DECLARE(const char *, gai_strerror, int ecode);
DECLARE(int, getnameinfo, const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags);
// macros for putc and getc seem to already create these
//DECLARE(int, _IO_getc, _IO_FILE *__fp);
//DECLARE(int, _IO_putc, int __c, _IO_FILE *__fp);

/******************************************************************************
**
** Called at library load time to initialize the function pointers
**
******************************************************************************/
void __attribute__ ((constructor)) xwrap_init(void)
{
	// NEED to do this first so tht it is loaded before the trace macros are called
	GET_FCN(fileno);
	GET_FCN(fileno_unlocked);
	GET_FCN(printf);
	GET_FCN(vprintf);

	MSG("loading XIA wrappers (created: %s)\n", __DATE__);

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
	GET_FCN(fprintf);
	GET_FCN(vfprintf);
	GET_FCN(vdprintf);
	GET_FCN(dprintf);
	GET_FCN(fgetc);
	GET_FCN(getc);
	GET_FCN(getchar);
//	GET_FCN(fclose);
	GET_FCN(getc_unlocked);
	GET_FCN(getchar_unlocked);
	GET_FCN(fgetc_unlocked);
	GET_FCN(fputc);
	GET_FCN(putc);
	GET_FCN(putchar);
	GET_FCN(fputc_unlocked);
	GET_FCN(putc_unlocked);
	GET_FCN(putchar_unlocked);
	GET_FCN(fgets);
	GET_FCN(gets);
	GET_FCN(fgets_unlocked);
	GET_FCN(getline);
	GET_FCN(fputs);
	GET_FCN(puts);
	GET_FCN(fread);
	GET_FCN(fwrite);
	GET_FCN(fputs_unlocked);
	GET_FCN(fread_unlocked);
	GET_FCN(fwrite_unlocked);
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
}
#endif // XIA_PRELOAD


/******************************************************************************
**
** Helper functions
**
******************************************************************************/

// call into the Xsockets API to see if the fd is associated with an Xsocket
#define isXsocket(s)	(getSocketType(s) != -1)

#define markWrapped(s)	(setWrapped(s, 1))
#define markUnwrapped(s)	(setWrapped(s, 0))

#define shouldWrap(s)	(isXsocket(s) && !isWrapped(s))

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
	}
	return XSOCK_INVALID;
}

/* 
** FIXME: all of the following functions should be modifed at some point
** to handle async socket calls
*/

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
	int rc;
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

/*
** The following are informational remappings, all they do is print the 
** parameter and return details
*/
int fileno(FILE *stream)
{
	int fd = __real_fileno(stream);	
//	TRACE("FILE:%p fd:%d\r\n", stream, fd);
	return fd;
}
int fileno_unlocked(FILE *stream)
{
	int fd = __real_fileno_unlocked(stream);
//	TRACE("FILE:%p fd:%d\r\n", stream, fd);
	return fd;
}

int dup(int fd)
{
	int fd2 = __real_dup(fd);
	TRACE("fd:%d fd2:%d\r\n");
	return fd2;
}

int dup2(int fd, int fd2)
{	
	TRACE("fd:%d fd2:%d\r\n", fd, fd2);
	return __real_dup2(fd, fd2);
}

int dup3(int fd, int fd2, int flags)
{
	TRACE("fd:%d fd2:%d flags:%d\r\n", fd, fd2, flags);
	return __real_dup3(fd, fd2, flags);
}

/*
 File I/O remappings start here
*/
int socket(int domain, int type, int protocol)
{
	int fd;
	FLAG();

	if (domain == AF_XIA) {
		XIAIFY();
		int async = type & SOCK_NONBLOCK;
		int coe = type & SOCK_CLOEXEC;
		TRACE("async:%d close on exec:%d\r\n", async, coe);
		// FIXME: handle flags somehow
		
		type = _GetSocketType(type & 0xf);
		fd = Xsocket(type);
	} else {
		fd = __real_socket(domain, type, protocol);
	}
	return fd;	
}

int socketpair(int domain, int type, int protocol, int fds[2])
{
	FLAG();
	if (domain == AF_XIA) {
		XIAIFY();
		int async = type & SOCK_NONBLOCK;
		int coe = type & SOCK_CLOEXEC;
		TRACE("async:%d close on exec:%d\r\n", async, coe);
		// FIXME: handle flags somehow

		type = _GetSocketType(type & 0xf);
		fds[0] = Xsocket(type);
		fds[1] = Xsocket(type);
		
		if (fds[0] >= 0 && fds[1] >= 0) {
			return 0;
		} else {
			Xclose(fds[0]);
			Xclose(fds[1]);
			return -1;
		}
		// FIXME
	} else {
		return __real_socketpair(domain, type, protocol, fds);
	}
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		// HACK!!!
		const char *dag = (const char *)addr;
		markWrapped(fd);
		rc= Xconnect(fd, dag);
		markUnwrapped(fd);

	} else {
		rc = __real_connect(fd, addr, len);
	}
	return rc;
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		// HACK!!!
		const char *dag = (const char *)addr;
		markWrapped(fd);
		rc= Xbind(fd, dag);
		markUnwrapped(fd);

	} else {
		rc = __real_bind(fd, addr, len);
	}
	return rc;
}

int accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
	int rc, rc1;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		// HACK!!!
		markWrapped(fd);
		rc = Xaccept(fd);
		if (rc >= 0) {
			char *dag = (char *)addr;
			rc1 = Xgetpeername(fd, dag, addr_len);
			if (rc1 < 0) {
				MSG("getpeername failed in accept");
			}
		}
		markUnwrapped(fd);

	} else {
		rc = __real_accept(fd, addr, addr_len);
	}
	return rc; 
}

int accept4(int fd, struct sockaddr *addr, socklen_t *addr_len, int flags)
{
	FLAG();
	return __real_accept4(fd, addr, addr_len, flags);
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	int rc;
	FLAG();

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
	FLAG();
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
	FLAG();

	if (0) {
//	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
		XIAIFY();
		// FIXME: need to add async support
		markWrapped(fd);
		rc = Xsendto(fd, buf, n, flags, (const char *)addr, addr_len);
		markUnwrapped(fd);

	} else {
		rc = __real_sendto(fd, buf, n, flags, addr, addr_len);
	}
	return rc;
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	int rc;
	FLAG();

	if (0) {
//	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
		XIAIFY();
		// FIXME: need to add async support		
		markWrapped(fd);
		rc = Xrecvfrom(fd, buf, n, flags, (char *)addr, addr_len);
		markUnwrapped(fd);
	} else {
		rc = __real_recvfrom(fd, buf, n, flags, addr, addr_len);
	}
	return rc;
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	FLAG();
	if (isXsocket(fd)) {
		ALERT();
		return 0;
		
	} else {
		return __real_sendmsg(fd, message, flags);
	}
}

ssize_t recvmsg(int fd, struct msghdr *message, int flags)
{
	FLAG();
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
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		/* NOTE: for now passing all options through and letting the
		** API decide if they are valid or not
		*/
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
	FLAG();	
	if (isXsocket(fd)) {
		XIAIFY();
		/* NOTE: for now passing all options through and letting the
		** API decide if they are valid or not
		*/
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
	FLAG();
	if (isXsocket(fd)) {
		SKIP();
		return 0;

	} else {	
		return __real_listen(fd, n);
	}
}

int close(int fd)
{
	int rc;
	FLAG();
	if (shouldWrap(fd)) {
//	if (isXsocket(fd) {
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
	FLAG();
	if (isXsocket(fd)) {
		SKIP();
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
	FLAG();
	if (isXsocket(fd)) {
		SKIP();
		return 0;

	} else {
		return __real_sockatmark(fd);
	}
}

int vfprintf(FILE *s, const char *format, va_list arg)
{
	int fd = fileno(s);
	int rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _vasend(fd, format, arg);

	} else {
		rc = __real_vfprintf(s, format, arg);
	}
	return rc;
}

int fprintf(FILE *stream, const char *format, ...)
{
	int rc;
	va_list args;	
	va_start(args, format);

	FLAG();
	rc = vfprintf(stream, format, args);
	va_end(args);
	return rc;
}

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

// NOTE: stdout may be remapped to our socket
int printf(const char *format, ...)
{
	int rc;
	va_list args;
	va_start(args, format);

	rc = vprintf(format, args);
	va_end(args);
	return rc;
}

int vdprintf(int fd, const char *fmt, va_list arg)
{
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _vasend(fd, fmt, arg);

	} else {
		NOXIA();
		rc = __real_vdprintf(fd, fmt, arg);
	}
	return rc;
}

int dprintf(int fd, const char *fmt, ...)
{
	int rc;
	va_list args;
	va_start(args, fmt);

	FLAG();
	rc = vdprintf(fd, fmt, args);
	va_end(args);
	return rc;
}

int fgetc(FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();

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
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getc(stream);
	}
	return rc;
}

int getchar(void)
{
	int fd = fileno(stdin);
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);
	
	} else {
		rc = __real_getc(stdin);
	}
	return rc;
}

int getc_unlocked(FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getc_unlocked(stream);
	}
	return rc;
}

int getchar_unlocked(void)
{
	int fd = fileno(stdin);
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_getchar_unlocked();
	}
	return rc;
}

int fgetc_unlocked(FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgetc(fd);

	} else {
		rc = __real_fgetc(stream);
	}
	return rc;
}

int fputc(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();

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
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putc(c, stream);
	}
	return rc;
}

int putchar(int c)
{
	int fd = fileno(stdout);
	int rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putchar(c);
	}
	return rc;
}

int fputc_unlocked(int c, FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();
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
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putc_unlocked(c, stream);
	}
	return rc;
}

int putchar_unlocked(int c)
{
	int fd = fileno(stdout);
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputc(c, fd);

	} else {
		rc = __real_putchar_unlocked(c);
	}
	return rc;
}

char *fgets(char *s, int n, FILE *stream)
{
	int fd = fileno(stream);
	char *rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, n);

	} else {
		rc = __real_fgets(s, n, stream);
	}
	return rc;
}

char *gets(char *s)
{
	int fd = fileno(stdin);
	char *rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, INT_MAX);

	} else {
		rc = __real_gets(s);
	}
	return rc;
}

char *fgets_unlocked(char *s, int n, FILE *stream)
{
	int fd = fileno(stream);
	char *rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xgets(fd, s, n);

	} else {
		rc = __real_fgets_unlocked(s, n, stream);
	}
	return rc;
}

_IO_ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	int fd = fileno(stream);
	_IO_ssize_t rc;
	
	FLAG();

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
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputs(s, fd);

	} else {
		rc = __real_fputs(s, stream);
	}
	return rc;
}

int fputs_unlocked(const char *s, FILE *stream)
{
	int fd = fileno(stream);
	int rc;
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xputs(s, fd);

	} else {
		rc = __real_fputs_unlocked(s, stream);
	}
	return rc;
}

int puts(const char *s)
{
	int fd = fileno(stdout);
	int rc;
	
	//FLAG();

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
	
	FLAG();

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
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfwrite(ptr, size, n, fd);

	} else {
		rc = __real_fwrite(ptr, size, n, s);
	}
	return rc;
}


size_t fread_unlocked(void *ptr, size_t size, size_t n, FILE *stream)
{
	int fd = fileno(stream);
	size_t rc;
	
	FLAG();

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
	
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		rc = _xfwrite(ptr, size, n, fd);

	} else {
		rc = __real_fwrite_unlocked(ptr, size, n, stream);
	}
	return rc;
}


ssize_t read(int fd, void *buf, size_t nbytes)
{
	size_t rc;
	
	FLAG();

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
	
	FLAG();

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
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		// HACK!!!
		char *dag = (char *)addr;
		markWrapped(fd);
		rc = Xgetsockname(fd, dag, len);
		markUnwrapped(fd);

	} else {
		rc = __real_getsockname(fd, addr, len);
	}
	return rc;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	int rc;
	
	FLAG();
	if (isXsocket(fd)) {
		XIAIFY();
		// HACK!!!
		char *dag = (char *)addr;
		markWrapped(fd);
		rc = Xgetpeername(fd, dag, len);
		markUnwrapped(fd);

	} else {
		return __real_getpeername(fd, addr, len);
	}
	return rc;
}

/******************************************************************************
** functions below still need to be implemented
******************************************************************************/

int ioctl(int d, int request, ...)
{
	int rc;
	va_list args;	
	
	FLAG();
	va_start(args, request);

	if (isXsocket(d)) {
		XIAIFY();
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
	
	FLAG();
	va_start(args, cmd);

	if (isXsocket(fd)) {
		XIAIFY();
		rc = -1;
	} else {
		NOXIA();
		rc = __real_fcntl(fd, cmd, args);
	}
	va_end(args);
	return rc;
}

#if 0
int fclose (FILE *stream)
{
	int fd = fileno(stream);
	int rc;

	FLAG();

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
	FLAG();

	MSG("not sure what to do with this function");
	// FIXME: what do we do with this??

	return __real_fcloseall();
}










int gethostname (char *name, size_t len)
{
	int rc;
	
	FLAG();

	rc = __real_gethostname(name, len);
	return rc;
}

int sethostname (const char *name, size_t len)
{
	int rc;
	
	FLAG();

	rc = __real_sethostname(name, len);
	return rc;
}

int sethostid (long int id)
{
	int rc;
	
	FLAG();

	rc = __real_sethostid(id);
	return rc;
}

int getdomainname (char *name, size_t len)
{
	int rc;
	
	FLAG();

	rc = __real_getdomainname(name, len);
	return rc;
}

int setdomainname (const char *name, size_t len)
{
	int rc;
	
	FLAG();

	rc = __real_setdomainname(name, len);
	return rc;
}

long int gethostid (void)
{
	FLAG();

	return __real_gethostid();
}



void sethostent (int stay_open)
{
	FLAG();

	__real_sethostent(stay_open);
}

void endhostent (void)
{
	FLAG();

	__real_endhostent();
}

struct hostent *gethostent (void)
{
	FLAG();
	return __real_gethostent();
}

struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type)
{
	FLAG();
	return __real_gethostbyaddr(addr, len, type);	
}

struct hostent *gethostbyname (const char *name)
{
	FLAG();
	return __real_gethostbyname(name);
}

struct hostent *gethostbyname2 (const char *name, int af)
{
	FLAG();
	return __real_gethostbyname2(name, af);
}

int gethostent_r (struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	FLAG();
	return __real_gethostent_r(result_buf, buf, buflen, result, h_errnop);
}

int gethostbyaddr_r (const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	FLAG();
	return __real_gethostbyaddr_r(addr, len, type, result_buf, buf, buflen, result, h_errnop);
}

int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	FLAG();
	return __real_gethostbyname_r(name, result_buf, buf, buflen, result, h_errnop);
}

int gethostbyname2_r (const char *name, int af, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop)
{
	FLAG();
	return __real_gethostbyname2_r(name, af, result_buf, buf, buflen, result, h_errnop);
}

void setnetent (int stay_open)
{
	FLAG();
	return __real_setnetent(stay_open);
}

void endnetent (void)
{
	FLAG();
	return __real_endnetent();
}

struct netent *getnetent (void)
{
	FLAG();
	return __real_getnetent();
}

struct netent *getnetbyaddr (uint32_t net, int type)
{
	FLAG();
	return __real_getnetbyaddr(net, type);
}

struct netent *getnetbyname (const char *name)
{
	FLAG();
	return __real_getnetbyname(name);
}

int getnetent_r (struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	FLAG();
	return __real_getnetent_r(result_buf, buf, buflen, result, h_errnop);
}

int getnetbyaddr_r (uint32_t net, int type, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	FLAG();
	return __real_getnetbyaddr_r(net, type, result_buf, buf, buflen, result, h_errnop);
}

int getnetbyname_r (const char *name, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop)
{
	FLAG();
	return __real_getnetbyname_r(name, result_buf, buf, buflen, result, h_errnop);
}

void setservent (int stay_open)
{
	FLAG();
	__real_setservent(stay_open);
}

void endservent (void)
{
	FLAG();
	__real_endservent();
}


struct servent *getservent (void)
{
	FLAG();
	return __real_getservent();
}

struct servent *getservbyname (const char *name, const char *proto)
{
	FLAG();
	return __real_getservbyname(name, proto);
}

struct servent *getservbyport (int port, const char *proto)
{
	FLAG();
	return __real_getservbyport(port, proto);
}

int getservent_r (struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	FLAG();
	return __real_getservent_r(result_buf, buf, buflen, result);
}

int getservbyname_r (const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	FLAG();
	return __real_getservbyname_r(name, proto, result_buf, buf, buflen, result);
}

int getservbyport_r (int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result)
{
	FLAG();
	return __real_getservbyport_r(port, proto, result_buf, buf, buflen, result);
}

void setprotoent (int stay_open)
{
	FLAG();
	__real_setprotoent(stay_open);
}

void endprotoent (void)
{
	FLAG();
	__real_endprotoent();
}

struct protoent *getprotoent (void)
{
	FLAG();
	return __real_getprotoent();
}

struct protoent *getprotobyname (const char *name)
{
	FLAG();
	return __real_getprotobyname(name);
}

struct protoent *getprotobynumber (int proto)
{
	FLAG();
	return __real_getprotobynumber(proto);
}

int getprotoent_r (struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	FLAG();
	return __real_getprotoent_r(result_buf, buf, buflen, result);
}

int getprotobyname_r (const char *name, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	FLAG();
	return __real_getprotobyname_r(name, result_buf, buf, buflen, result);
}

int getprotobynumber_r (int proto, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result)
{
	FLAG();
	return __real_getprotobynumber_r(proto, result_buf, buf, buflen, result);
}

int setnetgrent (const char *netgroup)
{
	FLAG();
	return __real_setnetgrent(netgroup);
}

void endnetgrent (void)
{
	FLAG();
	__real_endnetgrent();
}

int getnetgrent (char **hostp, char **userp, char **domainp)
{
	FLAG();
	return __real_getnetgrent(hostp, userp, domainp);
}

int innetgr (const char *netgroup, const char *host, const char *user, const char *domain)
{
	FLAG();
	return __real_innetgr(netgroup, host, user, domain);
}

int getnetgrent_r (char **hostp, char **userp, char **domainp, char *buffer, size_t buflen)
{
	FLAG();
	return __real_getnetgrent_r(hostp, userp, domainp, buffer, buflen);
}

int getaddrinfo (const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai)
{
	FLAG();
	return __real_getaddrinfo(name, service, req, pai);
}

void freeaddrinfo (struct addrinfo *ai)
{
	FLAG();
	return __real_freeaddrinfo(ai);
}

const char *gai_strerror (int ecode)
{
	FLAG();
	return __real_gai_strerror(ecode);
}

int getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags)
{
	int rc;
	FLAG();
	printf("__real_getnameinfo = %p\n", __real_getnameinfo);
	rc =__real_getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
	STOCK();
	return rc;
}

#if 0
/* 
** these seem to be handled by macros for putc and getc
*/
int _IO_getc (_IO_FILE *__fp)
{
	size_t rc;
	
	FLAG();

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
	
	FLAG();

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
** Informational remappings
** int dup (int fd);
** int dup2 (int fd, int fd2);
** int dup3 (int fd, int fd2, int flags);
** int fileno (FILE *stream);
** int fileno_unlocked (FILE *stream);
**
** Remapped file i/o functions
** int ioctl(int d, int request, ...);
** int fclose (FILE *stream);
** int fcloseall (void);
** int fprintf (FILE *stream, const char *format, ...);
** int printf (const char *format, ...);
** int vfprintf (FILE *s, const char *format, _G_va_list arg);
** int vprintf (const char *format, _G_va_list arg);
** int vdprintf (int fd, const char *fmt, _G_va_list arg);
** int dprintf (int fd, const char *fmt, ...);
** int fgetc (FILE *stream);
** int getc (FILE *stream);
** int getchar (void);
** int getc_unlocked (FILE *stream);
** int getchar_unlocked (void);
** int fgetc_unlocked (FILE *stream);
** int fputc (int c, FILE *stream);
** int putc (int c, FILE *stream);
** int putchar (int c);
** int fputc_unlocked (int c, FILE *stream);
** int putc_unlocked (int c, FILE *stream);
** int putchar_unlocked (int c);
** char *fgets (char *s, int n, FILE *stream)
** char *gets (char *s);
** char *fgets_unlocked (char *s, int n, FILE *stream);
** _IO_ssize_t getline (char **lineptr, size_t *n, FILE *stream);
** int fputs (const char *s, FILE *stream);
** int puts (const char *s);
** size_t fread (void *ptr, size_t size, size_t n, FILE *stream);
** size_t fwrite (const void *ptr, size_t size, size_t n, FILE *s);
** int fputs_unlocked (const char *s, FILE *stream);
** size_t fread_unlocked (void *ptr, size_t size, size_t n, FILE *stream);
** size_t fwrite_unlocked (const void *ptr, size_t size, size_t n, FILE *stream);
** ssize_t read (int fd, void *buf, size_t nbytes);
** ssize_t write (int fd, const void *buf, size_t n);
** int gethostname (char *name, size_t len);
** int sethostname (const char *name, size_t len)
** int sethostid (long int id);
** int getdomainname (char *name, size_t len);
** int setdomainname (const char *name, size_t len);
** long int gethostid (void);
** int fcntl (int fd, int cmd, ...);
** int _IO_getc (_IO_FILE *__fp);
** int _IO_putc (int __c, _IO_FILE *__fp);
**
** Remapped name functions
** void sethostent (int stay_open);
** void endhostent (void);
** struct hostent *gethostent (void);
** struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type);
** struct hostent *gethostbyname (const char *name);
** struct hostent *gethostbyname2 (const char *name, int af);
** int gethostent_r (struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
** int gethostbyaddr_r (const void *addr, socklen_t len, int type, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
** int gethostbyname_r (const char *name, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
** int gethostbyname2_r (const char *name, int af, struct hostent *result_buf, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
** void setnetent (int stay_open);
** void endnetent (void);
** struct netent *getnetent (void);
** struct netent *getnetbyaddr (uint32_t net, int type);
** struct netent *getnetbyname (const char *name);
** int getnetent_r (struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
** int getnetbyaddr_r (uint32_t net, int type, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
** int getnetbyname_r (const char *name, struct netent *result_buf, char *buf, size_t buflen, struct netent **result, int *h_errnop);
** void setservent (int stay_open);
** void endservent (void);
** struct servent *getservent (void);
** struct servent *getservbyname (const char *name, const char *proto);
** struct servent *getservbyport (int port, const char *proto);
** int getservent_r (struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
** int getservbyname_r (const char *name, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
** int getservbyport_r (int port, const char *proto, struct servent *result_buf, char *buf, size_t buflen, struct servent **result);
** void setprotoent (int stay_open);
** void endprotoent (void);
** struct protoent *getprotoent (void);
** struct protoent *getprotobyname (const char *name);
** struct protoent *getprotobynumber (int proto);
** int getprotoent_r (struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
** int getprotobyname_r (const char *name, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
** int getprotobynumber_r (int proto, struct protoent *result_buf, char *buf, size_t buflen, struct protoent **result);
** int setnetgrent (const char *netgroup);
** void endnetgrent (void);
** int getnetgrent (char **hostp, char **userp, char **domainp);
** int innetgr (const char *netgroup, const char *host, const char *user, const char *domain);
** int getnetgrent_r (char **hostp, char **userp, char **domainp, char *buffer, size_t buflen);
** int getaddrinfo (const char *name, const char *service, const struct addrinfo *req, struct addrinfo **pai);
** void freeaddrinfo (struct addrinfo *ai);
** const char *gai_strerror (int ecode);
** int getnameinfo (const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags);
**
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
** int snprintf (char *s, size_t maxlen, const char *format, ...);
** int vsnprintf (char *s, size_t maxlen, const char *format, _G_va_list arg);
** int vasprintf (char **ptr, const char *f, _G_va_list arg);
** int asprintf (char **ptr, const char *fmt, ...);
** int asprintf (char **ptr, const char *fmt, ...);
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
** 
** These functions probably won't be used in socket calls
** int fscanf (FILE *stream, const char *format, ...);
** int scanf (const char *format, ...);
** int sscanf (const char *s, const char *format, ...);
** int isoc99_fscanf (FILE *stream, const char *format, ...);
** int isoc99_scanf (const char *format, ...);
** int isoc99_sscanf (const char *s, const char *format, ...);
** int vfscanf (FILE *s, const char *format, _G_va_list arg)
** int vscanf (const char *format, _G_va_list arg);
** int isoc99_vfscanf (FILE *s, const char *format, _G_va_list arg);
** int isoc99_vscanf (const char *format, _G_va_list arg);
** int isoc99_vsscanf (const char *s, const char *format, _G_va_list arg);
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
