//#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

#define DEBUG
#define XIA_PRELOAD

#define isXsocket(s)	(getSocketType(s) != -1)

#ifdef DEBUG
#define MSG(...)	printf(__VA_ARGS__)
#define FLAG()		printf("%s called\n", __FUNCTION__)
#define XIAIFY()	printf("%s calling XIA functions\n", __FUNCTION__)
#define NOXIA()		printf("%s used normally\n", __FUNCTION__)
#define SKIP()		printf("%s NOT XIAified\n", __FUNCTION__)
#define STOCK()		printf("%s has not been changed yet\n", __FUNCTION__)
#else
#define MSG(...)
#define FLAG()
#define XIAIFY()
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
** LD_PRELOAD=xwrap.so application
*/
#ifdef XIA_PRELOAD
// a Couple of macros to save lots of typing

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
**
** FIXME: see if there's a way to make this look a little more like a normal 
**	function declaration.
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


/*
** Called at library load time to initialize the function pointers
*/
void __attribute__ ((constructor)) xwrap_init(void)
{
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
}

#endif // XIA_PRELOAD


int socket(int domain, int type, int protocol)
{
	FLAG();
	STOCK();

	if (domain == AF_XIA) {
		XIAIFY();
		return Xsocket(type);
	} else {
		NOXIA();
		return __real_socket(domain, type, protocol);
	}
}

int socketpair(int domain, int type, int protocol, int fds[2])
{
	FLAG();
	STOCK();
	
	return __real_socketpair(domain, type, protocol, fds);
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	FLAG();
	STOCK();

	// FIXME: need to modify structsockaddr to work with DAGS

	return __real_bind(fd, addr, len);
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	FLAG();
	STOCK();
	return __real_getsockname(fd, addr, len);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	FLAG();
	STOCK();
	return __real_connect(fd, addr, len);
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	FLAG();
	STOCK();
	return __real_getpeername(fd, addr, len);
}

ssize_t send(int fd, const void *buf, size_t n, int flags)
{
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		return Xsend(fd, buf, n, flags);
	} else {
		NOXIA();
		return __real_send(fd, buf, n, flags);
	}
}

ssize_t recv(int fd, void *buf, size_t n, int flags)
{
	FLAG();
	STOCK();
	return __real_recv(fd, buf, n, flags);
}

ssize_t sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
{
	FLAG();
	STOCK();

	if (isXsocket(fd)) {
		XIAIFY();
		return Xsendto(fd, buf, n, flags, (const char *)addr, addr_len);
	} else {
		NOXIA();
		return __real_sendto(fd, buf, n, flags, addr, addr_len);
	}
}

ssize_t recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	FLAG();
	STOCK();

	if (isXsocket(fd)) {
		XIAIFY();
		return Xrecvfrom(fd, buf, n, flags, (char *)addr, addr_len);
	} else {
		NOXIA();
		return __real_recvfrom(fd, buf, n, flags, addr, addr_len);
	}
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	FLAG();
	STOCK();
	MSG("should we modify this function, I've never seen it used\n");
	return __real_sendmsg(fd, message, flags);
}

ssize_t recvmsg (int fd, struct msghdr *message, int flags)
{
	FLAG();
	STOCK();
	MSG("should we modify this function, I've never seen it used\n");
	return __real_recvmsg(fd, message, flags);	
}

int getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
	FLAG();
	STOCK();
	return __real_getsockopt(fd, level, optname, optval, optlen);
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	FLAG();
	STOCK();
	return __real_setsockopt(fd, level, optname, optval, optlen);
}

int listen(int fd, int n)
{
	FLAG();
	STOCK();
	return __real_listen(fd, n);
}

int accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
	FLAG();
	STOCK();
	return __real_accept(fd, addr, addr_len);
}

int accept4(int fd, struct sockaddr *addr, socklen_t *addr_len, int flags)
{
	FLAG();
	STOCK();
	return __real_accept4(fd, addr, addr_len, flags);
}

int shutdown(int fd, int how)
{
	FLAG();
	STOCK();
	return __real_shutdown(fd, how);
}

int sockatmark(int fd)
{
	FLAG();
	STOCK();
	return __real_sockatmark(fd);
}

int close(int fd)
{
	FLAG();

	if (isXsocket(fd)) {
		XIAIFY();
		return Xclose(fd);
	} else {
		NOXIA();
		return __real_close(fd);
	}
}
