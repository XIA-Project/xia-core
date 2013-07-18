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

/* Rewritten for XIA */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

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

#define DEFAULT_SID  "SID:ffff000000000000000000000000000123456789"
#define DEFAULT_NAME "xnc_s.testbed.xia"

#define vwrite (ssize_t (*)(int, void *, size_t))write

/* Command Line Options */
int	dflag = 0;					/* detached, no stdin */
int	hflag = 0;					/* detached, no stdin */
int	Iflag = 0;					/* TCP receive buffer size */
int iflag = 0;				/* Interval Flag */
int	kflag = 0;					/* More than one connect */
int	lflag = 0;					/* Bind to local port */
int	nflag = 0;					/* Don't do name look up */
int	Oflag = 0;					/* TCP send buffer size */
int	qflag = 0;					/* Don't do name look up */
int	uflag = 0;					/* UDP - Default to TCP */
int	vflag = 0;					/* Verbosity */
int	wflag = 0;					/* Verbosity */

void	help(void);
int	local_listen(const char *, const char *, int transport_type);
void	readwrite(int, sockaddr_x *, int transport_type);
int	remote_connect(const char *, sockaddr_x *, int transport_type);
int	udptest(int);
void	report_connect(const struct sockaddr *, socklen_t);
void	usage(int);
size_t	atomicio(ssize_t (*)(int, void *, size_t), int, void *, size_t);

    int timeout = -1;

int
main(int argc, char *argv[])
{
	int ch, ret = 1;
	const char *host = NULL, *name = NULL;
    int transport_type = SOCK_DGRAM;
    int sock = 0;

    int buf_size = 0;

	while ((ch = getopt(argc, argv, "dhI:i:klnO:q:uvw:")) >= 0)
    {
		switch (ch)
        {
            case 'd':
                dflag = 1;
                break;
            case 'h':
                hflag = 1;
                help();
                break;
            case 'I':
                Iflag = 1;
                //Iflag = strtonum(optarg, 1, 65536 << 14, &errstr);
                //if (errstr != NULL)
                    //errx(1, "TCP receive window %s: %s",
                        //errstr, optarg);
                break;
            case 'i':
                iflag = 1;
                //iflag = strtonum(optarg, 0, UINT_MAX, &errstr);
                //if (errstr)
                    //errx(1, "interval %s: %s", errstr, optarg);
                break;
            case 'k':
                kflag = 1;
                break;
            case 'l':
                lflag = 1;
                break;
            case 'n':
                nflag = 1;
                break;
            case 'O':
                Oflag = 1;
                //Oflag = strtonum(optarg, 1, 65536 << 14, &errstr);
                //if (errstr != NULL)
                    //errx(1, "TCP send window %s: %s",
                        //errstr, optarg);
                break;
            case 'q':
                qflag = 1;
                break;
            case 'u':
                uflag = 1;
                break;
            case 'v':
                vflag = 1;
                break;
            case 'w':
                wflag = 1;
                //timeout = strtonum(optarg, 0, INT_MAX / 1000, &errstr);
                //if (errstr)
                    //errx(1, "timeout %s: %s", errstr, optarg);
                //timeout *= 1000;
                break;
            default:
                usage(1);
                break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0])
		host = argv[0];
	else
		usage(1);

	if (!lflag && kflag)
		errx(1, "must use -l with -k");

    transport_type = uflag ? SOCK_DGRAM : SOCK_STREAM;

	if (lflag) {
		ret = 0;

        printf("Listening as server\n");

		/* Allow only one connection at a time, but stay alive. */
		for (;;) {
            name = DEFAULT_SID;
            sock = local_listen(name, host, transport_type);
			if (sock < 0)
				err(1, NULL);
			/*
			 * For UDP and -k, don't connect the socket, let it
			 * receive datagrams from multiple socket pairs.
			 */
			if (uflag && kflag)
            {
                printf("XDP\n");
				readwrite(sock, NULL, transport_type);
            }
			/*
			 * For UDP and not -k, we will use recvfrom() initially
			 * to wait for a caller, then use the regular functions
			 * to talk to the caller.
			 */
			else if (uflag && !kflag) {
                printf("XDP\n");

				readwrite(sock, NULL, transport_type);

			} else {

                printf("XSP\n");

                int s2;

                if ((s2 = Xaccept(sock, NULL, 0)) < 0) {
                    printf("could not accept XSP connection");
                    exit(1);
                }

                Xclose(sock);

				readwrite(s2, NULL, transport_type);
			}

            Xclose(sock);

			if (uflag) {
				if (connect(sock, NULL, 0) < 0)
					err(1, "connect");
			}

			if (!kflag)
				break;
		}
	} else {
        printf("Connecting as client\n");
        transport_type == SOCK_DGRAM ? printf("XDP\n") :  printf("XSP\n");

        sockaddr_x client;

        sock = remote_connect(host, &client, transport_type);
        if (sock <0)
        {
            printf("remote connection failed\n");
            exit(1);
        }

        readwrite(sock, &client, transport_type);
	}

	if (sock)
		Xclose(sock);

	exit(ret);
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed. Returns -1 on failure.
 */
int
remote_connect(const char *host, sockaddr_x *client, int transport_type)
{
    int sock;
    socklen_t len;

    // create a datagram socket
    if ((sock = Xsocket(AF_XIA, transport_type, 0)) < 0) {
        printf("error: unable to create the listening socket.\n");
        return -1;
    }

    len = sizeof(*client);
    if (XgetDAGbyName(host, client, &len) < 0) {
        printf("unable to locate: %s\n", host);
        return -1;
    }

    if (transport_type == SOCK_STREAM)
    {
        if (Xconnect(sock, (struct sockaddr*)client, len) < 0)
        {
            printf("connection failed\n");
            exit(1);
        }
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
		printf("error: unable to bind to %s\n", host);
		exit(1);
	}

    return sock;
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(int nfd, sockaddr_x *c2, int transport_type)
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

            if (transport_type == SOCK_DGRAM)
            {
                n = Xrecvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &len);
            }
            else
            {
                n = Xrecv(sock, buf, sizeof(buf), 0);
            }

            if (n < 0) {
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
				pfd[1].fd = -1;
				pfd[1].events = 0;
			} else {
                if (transport_type == SOCK_DGRAM)
                {
                    if (Xsendto(sock, buf, n, 0, (struct sockaddr*)&client, sizeof(client)) < 0)
                    {
                        printf("error sending time to the client\n");
                        return;
                    }
                }
                else
                {
                    if (Xsend(sock, buf, n, 0) < 0)
                    {
                        printf("error sending time to the client\n");
                        return;
                    }
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
