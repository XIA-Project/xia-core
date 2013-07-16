/* $OpenBSD: netcat.c,v 1.112 2013/04/29 00:28:23 okan Exp $ */
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Re-written nc(1) for OpenBSD. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/telnet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "Xsocket.h"
#include "dagaddr.hpp"

#define SID0 "SID:0f00000000000000000000000000000123456789"
#define SNAME "tod_s.testbed.xia"

#define PORT_MAX	65535
#define PORT_MAX_LEN	6

#define vwrite (ssize_t (*)(int, void *, size_t))write

/* Command Line Options */
int	dflag;					/* detached, no stdin */
unsigned int iflag;				/* Interval Flag */
int	kflag;					/* More than one connect */
int	lflag;					/* Bind to local port */
int	Nflag;					/* shutdown() network socket */
int	nflag;					/* Don't do name look up */
char   *pflag;					/* Localport flag */
int	rflag;					/* Random ports flag */
char   *sflag;					/* Source Address */
int	uflag;					/* UDP - Default to TCP */
int	vflag;					/* Verbosity */
int	zflag;					/* Port Scan Flag */
int	Dflag;					/* sodebug */
int	Iflag;					/* TCP receive buffer size */
int	Oflag;					/* TCP send buffer size */
int	Sflag;					/* TCP MD5 signature option */
u_int	rtableid;

int timeout = -1;
int family = AF_UNSPEC;
char *portlist[PORT_MAX+1];

void	help(void);
int	local_listen(const char *, const char *, int transport_type);
void	readwrite(int, sockaddr_x *);
int	remote_connect(const char *, sockaddr_x *);
int	udptest(int);
void	report_connect(const struct sockaddr *, socklen_t);
void	usage(int);

/*
 * Ensure all of data on socket comes through. f==read || f==vwrite
 */
size_t	atomicio(ssize_t (*)(int, void *, size_t), int, void *, size_t);

int
main(int argc, char *argv[])
{
	int ch, s, ret;
	const char *host, *name;
    int transport_type = SOCK_DGRAM;

	ret = 1;
	s = 0;
	host = NULL;

	while ((ch = getopt(argc, argv,
	    "46DdhI:i:klNnO:P:p:rSs:tT:UuV:vw:X:x:z")) != -1) {
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		case 'h':
			help();
			break;
		/*case 'i':
			iflag = strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr)
				errx(1, "interval %s: %s", errstr, optarg);
			break;*/
		case 'k':
			kflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = optarg;
			break;
		case 'u':
			uflag = 1;
			break;
		/*case 'V':
			rtableid = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable %s: %s", errstr, optarg);
			break;*/
		case 'v':
			vflag = 1;
			break;
		/*case 'w':
			timeout = strtonum(optarg, 0, INT_MAX / 1000, &errstr);
			if (errstr)
				errx(1, "timeout %s: %s", errstr, optarg);
			timeout *= 1000;
			break;*/
		case 'z':
			zflag = 1;
			break;
		case 'D':
			Dflag = 1;
			break;
		/*case 'I':
			Iflag = strtonum(optarg, 1, 65536 << 14, &errstr);
			if (errstr != NULL)
				errx(1, "TCP receive window %s: %s",
				    errstr, optarg);
			break;*/
		/*case 'O':
			Oflag = strtonum(optarg, 1, 65536 << 14, &errstr);
			if (errstr != NULL)
				errx(1, "TCP send window %s: %s",
				    errstr, optarg);
			break;*/
		case 'S':
			Sflag = 1;
			break;
		default:
			usage(1);
            break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0] && !argv[1]) {
		if  (!lflag)
			usage(1);
		host = NULL;
	} else if (argv[0] && argv[1]) {
		host = argv[0];
	} else
		usage(1);

	if (lflag && sflag)
		errx(1, "cannot use -s and -l");
	if (lflag && pflag)
		errx(1, "cannot use -p and -l");
	if (lflag && zflag)
		errx(1, "cannot use -z and -l");
	if (!lflag && kflag)
		errx(1, "must use -l with -k");

	/* Initialize addrinfo structure. */
    /*
	if (family != AF_UNIX) {
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
		hints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
		if (nflag)
			hints.ai_flags |= AI_NUMERICHOST;
	}*/

	if (lflag) {
		ret = 0;

        printf("Listening as server\n");

		/* Allow only one connection at a time, but stay alive. */
		for (;;) {
            name = SID0;
            host = SNAME;
            s = local_listen(name, host, transport_type);
			if (s < 0)
				err(1, NULL);
			/*
			 * For UDP and -k, don't connect the socket, let it
			 * receive datagrams from multiple socket pairs.
			 */
			if (uflag && kflag)
            {
                printf("UDP\n");
				readwrite(s, NULL);
            }
			/*
			 * For UDP and not -k, we will use recvfrom() initially
			 * to wait for a caller, then use the regular functions
			 * to talk to the caller.
			 */
			else if (uflag && !kflag) {
                printf("UDP\n");

				readwrite(s, NULL);

			} else {

                printf("TCP\n");

                /*

				len = sizeof(cliaddr);
				connfd = accept(s, (struct sockaddr *)&cliaddr,
				    &len);
				if (connfd == -1) {
					// For now, all errnos are fatal
   					err(1, "accept");
				}
				if (vflag)
					report_connect((struct sockaddr *)&cliaddr, len);

				readwrite(connfd);
				close(connfd);
                */
			}

			if (family != AF_UNIX)
				Xclose(s);
			else if (uflag) {
				if (connect(s, NULL, 0) < 0)
					err(1, "connect");
			}

			if (!kflag)
				break;
		}
	} else {
        printf("Connecting as client\n");
        printf("UDP\n");

        sockaddr_x client;

        s = remote_connect(SNAME, &client);

        readwrite(s, &client);
	}

	if (s)
		Xclose(s);

	exit(ret);
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed. Returns -1 on failure.
 */
int
remote_connect(const char *host, sockaddr_x *client)
{
    int sock;
    socklen_t len;

    // create a datagram socket
    if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
        printf("error: unable to create the listening socket.\n");
        return -1;
    }

    len = sizeof(*client);
    if (XgetDAGbyName(host, client, &len) < 0) {
        printf("unable to locate: %s\n", SNAME);
        return -1;
    }

	return (sock);
}

/*
 * local_listen()
 * Returns a socket listening on a local port, binds to specified source
 * address. Returns -1 on failure.
 */
int
local_listen(const char *sid, const char *host, int transport_type)
{
    int sock;
	struct addrinfo *ai;
    sockaddr_x *sa;

    // create a datagram socket
    if ((sock = Xsocket(AF_XIA, transport_type, 0)) < 0) {
		printf("error: unable to create the listening socket.\n");
		exit(1);
	}

	if (Xgetaddrinfo(NULL, sid, NULL, &ai) != 0) {
    	printf("error: unable to create source dag.");
		exit(1);
	}

    sa = (sockaddr_x*)ai->ai_addr;

    //Register this service name to the name server
    if (XregisterName(host, sa) < 0) {
    	printf("error: unable to register name/dag combo");
		exit(1);
	}

    // bind to the DAG
    if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		printf("error: unable to bind to %s\n", SNAME);
		exit(1);
	}

    return sock;
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(int nfd, sockaddr_x *c2)
{
	struct pollfd pfd[2];
	int n, wfd = fileno(stdin);
	int lfd = fileno(stdout);
	int plen;

    int sock;
    socklen_t len;
    char buf[XIA_MAXBUF];
    sockaddr_x client;

	plen = 2048;

    if (c2 != NULL)
        client = *c2;

	/* Setup Network FD */
	pfd[0].fd = nfd;
	pfd[0].events = POLLIN;

	/* Set up STDIN FD. */
	pfd[1].fd = wfd;
	pfd[1].events = POLLIN;

    sock = nfd;

	while (pfd[0].fd != -1) {
		if (iflag)
			sleep(iflag);

		if ((n = poll(pfd, 2 - dflag, timeout)) < 0) {
			close(nfd);
			err(1, "Polling Error");
		}

		if (n == 0)
			return;

		if (pfd[0].revents & POLLIN) {
            len = sizeof(client);

            if ((n = Xrecvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &len)) < 0) {
                printf("error receiving client request\n");
                // assume it's ok, and just keep listening
                //return;
            }
			else if (n == 0) {
                printf("shutting down\n");
				//shutdown(nfd, SHUT_RD);
				pfd[0].fd = -1;
				pfd[0].events = 0;
			} else {
				if (atomicio(vwrite, lfd, buf, n) != (size_t)n)
					return;
			}
		}

		if (!dflag && pfd[1].revents & POLLIN) {
			if ((n = read(wfd, buf, plen)) < 0)
				return;
			else if (n == 0) {
				if (Nflag)
					shutdown(nfd, SHUT_WR);
				pfd[1].fd = -1;
				pfd[1].events = 0;
			} else {
                if (Xsendto(sock, buf, n, 0, (struct sockaddr*)&client, sizeof(client)) < 0)
                {
                    printf("error sending time to the client\n");
                    return;
                }
			}
		}
	}
}

/*
 * udptest()
 * Do a few writes to see if the UDP port is there.
 * Fails once PF state table is full.
 */
int
udptest(int s)
{
	int i, ret;

	for (i = 0; i <= 3; i++) {
		if (write(s, "X", 1) == 1)
			ret = 1;
		else
			ret = -1;
	}
	return (ret);
}

void
report_connect(const struct sockaddr *sa, socklen_t salen)
{
	char remote_host[NI_MAXHOST];
	char remote_port[NI_MAXSERV];
	int herr;
	int flags = NI_NUMERICSERV;
	
	if (nflag)
		flags |= NI_NUMERICHOST;
	
	if ((herr = getnameinfo(sa, salen,
	    remote_host, sizeof(remote_host),
	    remote_port, sizeof(remote_port),
	    flags)) != 0) {
		if (herr == EAI_SYSTEM)
			err(1, "getnameinfo");
		else
			errx(1, "getnameinfo: %s", gai_strerror(herr));
	}
	
	fprintf(stderr,
	    "Connection from %s %s "
	    "received!\n", remote_host, remote_port);
}

void
help(void)
{
	usage(0);
	fprintf(stderr, "\tCommand Summary:\n\
	\t-4		Use IPv4\n\
	\t-6		Use IPv6\n\
	\t-D		Enable the debug socket option\n\
	\t-d		Detach from stdin\n\
	\t-h		This help text\n\
	\t-I length	TCP receive buffer length\n\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
	\t-k		Keep inbound sockets open for multiple connects\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-N		Shutdown the network socket after EOF on stdin\n\
	\t-n		Suppress name/port resolutions\n\
	\t-O length	TCP send buffer length\n\
	\t-p port\t	Specify local port for remote connects\n\
	\t-r		Randomize remote ports\n\
	\t-S		Enable the TCP MD5 signature option\n\
	\t-s addr\t	Local source address\n\
	\t-T toskeyword\tSet IP Type of Service\n\
	\t-t		Answer TELNET negotiation\n\
	\t-U		Use UNIX domain socket\n\
	\t-u		UDP mode\n\
	\t-V rtable	Specify alternate routing table\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n\
	\t-z		Zero-I/O mode [used for scanning]\n\
	Port numbers can be individual or ranges: lo-hi [inclusive]\n");
	exit(1);
}

void
usage(int ret)
{
	fprintf(stderr,
	    "usage: xnetcat [-46DdhklNnrStUuvz] [-I length] [-i interval] [-O length]\n"
	    "\t  [-p source_port] [-s source] [-T ToS]\n"
	    "\t  [-V rtable] [-w timeout]\n"
	    "\t  [destination] [port]\n");
	if (ret)
		exit(1);
}

/*
 * ensure all of data on socket comes through. f==read || f==vwrite
 */
size_t
atomicio(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n)
{
	char *s = (char *)_s;
	size_t pos = 0;
	ssize_t res;
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = f == read ? POLLIN : POLLOUT;
	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR)
				continue;
			if ((errno == EAGAIN) || (errno == ENOBUFS)) {
				(void)poll(&pfd, 1, -1);
				continue;
			}
			return 0;
		case 0:
			errno = EPIPE;
			return pos;
		default:
			pos += (size_t)res;
		}
	}
	return (pos);
}
