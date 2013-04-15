/* See LICENSE file for license details. */
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "Xsocket.h"

static void
eprint(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s", bufout);
	if(fmt[0] && fmt[strlen(fmt) - 1] == ':')
		fprintf(stderr, " %s\n", strerror(errno));
	exit(1);
}

static int
dial(char *service) {
	int sock;
	sockaddr_x sa;
	socklen_t slen;

	if ((sock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		eprint("error: unable to open Xsocket\n");
		return sock;
	}

	slen = sizeof(sa);
	if (XgetDAGbyName(service, &sa, &slen) < 0) {
		Xclose(sock);
		eprint("error: unable to resovle service '%s'.\n", host);
		return -1;
	}

	if (Xconnect(sock, (struct sockaddr*)&sa, slen) < 0) {
		Xclose(sock);
		eprint("error: unable to connect to service '%s'.\n", host);
		return -1;
	}

	return sock;
}

#define strlcpy _strlcpy
static void
strlcpy(char *to, const char *from, int l) {
	memccpy(to, from, '\0', l);
	to[l-1] = '\0';
}

static char *
eat(char *s, int (*p)(int), int r) {
	while(s != '\0' && p(*s) == r)
		s++;
	return s;
}

static char*
skip(char *s, char c) {
	while(*s != c && *s != '\0')
		s++;
	if(*s != '\0')
		*s++ = '\0';
	return s;
}

static void
trim(char *s) {
	char *e;

	e = s + strlen(s) - 1;
	while(isspace(*e) && e > s)
		e--;
	*(e + 1) = '\0';
}
