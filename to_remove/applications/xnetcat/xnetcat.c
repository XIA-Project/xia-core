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

/* Modified by Vikram Rajkumar <vrajkuma@andrew.cmu.edu> for XIA */

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

void	usage(int);
void	help(void);
int	    local_listen(const char *, const char *);
int	    remote_connect(const char *, sockaddr_x *);
void	readwrite(int, sockaddr_x *);
size_t	atomicio(ssize_t (*)(int, void *, size_t), int, void *, size_t);

/* Command Line Options */
int	dflag = 0;				/* detached, no stdin */
int	kflag = 0;				/* More than one connect */
int	lflag = 0;				/* Listen */
int	vflag = 0;				/* Verbosity */

int transport_type = SOCK_STREAM;

size_t input_buffer_size = XIA_MAXBUF;
size_t output_buffer_size = XIA_MAXBUF;

unsigned int interval_delay = 0;
unsigned int wait_timeout = -1;

int main(int argc, char *argv[])
{
	int ch;

    char *sid = (char *)DEFAULT_SID;
    char *name = (char *)DEFAULT_NAME;

    int socket = 0;
    sockaddr_x addr;

    if (argc <= 1)
        usage(1);

	while ((ch = getopt(argc, argv, "dhI:i:klNnO:uvw:")) >= 0)
    {
		switch (ch)
        {
            case 'd':
                dflag = 1;
                break;
            case 'h':
                help();
                break;
            case 'I':
                input_buffer_size = strtol(optarg, NULL, 0);
                break;
            case 'i':
                interval_delay = strtol(optarg, NULL, 0);
                break;
            case 'k':
                kflag = 1;
                break;
            case 'l':
                lflag = 1;
                break;
            case 'O':
                output_buffer_size = strtol(optarg, NULL, 0);
                break;
            case 'u':
                transport_type = SOCK_DGRAM;
                break;
            case 'v':
                vflag = 1;
                break;
            case 'w':
                wait_timeout = strtol(optarg, NULL, 0);
                break;
            default:
                usage(1);
                break;
		}
	}

	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (kflag && !lflag)
		errx(1, "must use -l with -k");

    if (argc > 2)
    {
        usage(1);
    }
    else if (argc == 2)
    {
        sid = argv[0];
        name = argv[1];
    }
    else if (argc == 1)
    {
        name = argv[0];
    }

	if (lflag)
    {
		/* Allow only one connection at a time, but stay alive. */
        while (1)
        {
            if ((socket = local_listen(sid, name)) < 0)
                errx(1, "unable to open listening socket");

            readwrite(socket, &addr);

            Xclose(socket);

			if (!kflag)
				break;
		}
	}
    else
    {
        if ((socket = remote_connect(name, &addr)) < 0)
            errx(1, "remote connection failed");

        readwrite(socket, &addr);

		Xclose(socket);
	}

	exit(0);
}

void usage(int ret)
{
	fprintf(stderr,
	    "usage: xnetcat [-dhkluv] [-I length] [-i interval] [-O length] [-w timeout]\n"
	    "\t  [service_name]\n");

	if (ret)
		exit(1);
}

void help()
{
	usage(0);

	fprintf(stderr, "\tCommand Summary:\n\
	\t-d		Detach from stdin\n\
	\t-h		This help text\n\
	\t-I length	XSP receive buffer length\n\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
	\t-k		Keep accepting inbound connections\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-O length	XSP send buffer length\n\
	\t-u		XDP mode\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n");

	exit(1);
}

/*
 * local_listen()
 * Returns a socket bound to a specific SID. Returns -1 on failure.
 */
int local_listen(const char *sid, const char *name)
{
    int socket = 0;
	struct addrinfo *ai = NULL;
    sockaddr_x *sa = NULL;
    int stream_socket = 0;

    if ((socket = Xsocket(AF_XIA, transport_type, 0)) < 0)
    {
		printf("error: unable to create the listening socket\n");
        return -1;
	}

	if (Xgetaddrinfo(NULL, sid, NULL, &ai) != 0)
    {
        Xclose(socket);
    	printf("error: unable to create source dag\n");
        return -1;
	}

    sa = (sockaddr_x *)ai->ai_addr;

    // Register this service name to the name server
    if (XregisterName(name, sa) < 0)
    {
        Xfreeaddrinfo(ai);
        Xclose(socket);
    	printf("error: unable to register name %s to sid %s\n", name, sid);
        return -1;
	}

    // Bind to the DAG
    if (Xbind(socket, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0)
    {
        Xfreeaddrinfo(ai);
        Xclose(socket);
		printf("error: unable to bind to %s\n", name);
        return -1;
	}

    Xfreeaddrinfo(ai);

    if (transport_type == SOCK_STREAM)
    {
        if ((stream_socket = Xaccept(socket, NULL, 0)) < 0)
        {
            Xclose(socket);
            printf("error: unable to accept XSP connection\n");
            return -1;
        }

        Xclose(socket);

        socket = stream_socket;
    }

    return socket;
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Returns -1 on failure.
 */
int remote_connect(const char *name, sockaddr_x *addr)
{
    int socket;
    socklen_t len = sizeof(sockaddr_x);

    if ((socket = Xsocket(AF_XIA, transport_type, 0)) < 0)
    {
        printf("error: unable to create the listening socket\n");
        return -1;
    }

    if (XgetDAGbyName(name, addr, &len) < 0)
    {
        Xclose(socket);
        printf("error: unable to locate %s\n", name);
        return -1;
    }

    if (transport_type == SOCK_STREAM)
    {
        if (Xconnect(socket, (struct sockaddr *)addr, len) < 0)
        {
            Xclose(socket);
            printf("error: connection failed\n");
            return -1;
        }
    }

	return socket;
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void readwrite(int socket, sockaddr_x *addr)
{
	struct pollfd pfd[2];
	int wfd = fileno(stdin);
	int lfd = fileno(stdout);
    int n = 0;

    socklen_t len = sizeof(sockaddr_x);

    char input_buffer[input_buffer_size];
    char output_buffer[output_buffer_size];

	/* Setup Network FD */
	pfd[0].fd = socket;
	pfd[0].events = POLLIN;

	/* Set up STDIN FD. */
	pfd[1].fd = wfd;
	pfd[1].events = POLLIN;

	while (pfd[0].fd != -1)
    {
		if (interval_delay)
			sleep(interval_delay);

		if ((n = poll(pfd, 2 - dflag, wait_timeout)) < 0)
        {
            printf("polling error\n");
            return;
		}

		if (n == 0)
			return;

		if (pfd[0].revents & POLLIN)
        {
            if (transport_type == SOCK_STREAM)
            {
                n = Xrecv(socket, input_buffer, input_buffer_size, 0);
            }
            else
            {
                n = Xrecvfrom(socket, input_buffer, input_buffer_size, 0, (struct sockaddr *)addr, &len);
            }

            if (n < 0)
            {
                printf("error receiving client request\n");
                return;
            }
			else if (n == 0)
            {
				pfd[0].fd = -1;
				pfd[0].events = 0;
			}
            else
            {
				if (atomicio(vwrite, lfd, input_buffer, n) != (size_t)n)
					return;
			}
		}

		if (!dflag && pfd[1].revents & POLLIN)
        {
			if ((n = read(wfd, output_buffer, output_buffer_size)) < 0)
            {
				return;
            }
			else if (n == 0)
            {
				pfd[1].fd = -1;
				pfd[1].events = 0;
			}
            else
            {
                if (transport_type == SOCK_STREAM)
                {
                    if (Xsend(socket, output_buffer, n, 0) < 0)
                    {
                        printf("error sending response to the client\n");
                        return;
                    }
                }
                else
                {
                    if (Xsendto(socket, output_buffer, n, 0, (struct sockaddr *)addr, len) < 0)
                    {
                        printf("error sending response to the client\n");
                        return;
                    }
                }
			}

            if (pfd[1].revents & POLLHUP)
                return;
		}
	}
}

/*
 * Ensure all of data on socket comes through.
 * f == read || f == vwrite
 */
size_t atomicio(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n)
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
