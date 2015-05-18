

#include <sys/poll.h>	
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "Xsocket.h"

int test(unsigned nfds, fd_set *rfd, fd_set *wfd, fd_set *efd, struct timeval *timeout)
{
	int e;
	printf("nfds:%d timeout:%ld:%ld\n", nfds, timeout->tv_sec, timeout->tv_usec);

	time_t start = time(NULL);
	errno = 0;
	int rc = Xselect(nfds, rfd, wfd, efd, timeout);
	e = errno;
	time_t end = time(NULL);

	printf("rc:%d errno:%d, elapsed:%d\n", rc, e, (int)(end - start));

	if (rc > 0) {
		for (unsigned i = 0; i < nfds; i++) {
			if (rfd && FD_ISSET(i, rfd))
				printf("sock %d read ready\n", i);
			if (wfd && FD_ISSET(i, wfd))
				printf("sock %d write ready\n", i);
			if (efd && FD_ISSET(i, efd))
				printf("sock %d error ready\n", i);
		}
	}
	printf("\n");

	return rc;
}

int main()
{
	fd_set rfd, wfd, efd;
	struct timeval tv;
	int fd1, fd2, fd3, fd4;

printf("POLLIN:%d POLLOUT%d POLLERR:%d\n", POLLIN, POLLOUT, POLLERR);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	printf("bad parameters\n");
	test(1, NULL, NULL, NULL, &tv);

	printf("just a timeout\n");
	test(0, NULL, NULL, NULL, &tv);

	FD_SET(10, &rfd);
	printf("invlid socket\n");
	test(11, &rfd, NULL, NULL, &tv);

	fd1 = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	FD_ZERO(&rfd);
	FD_SET(fd1, &rfd);
printf("socket 0 = %d\n", fd1);
	printf("single socket read check\n");
	test(fd1 + 1, &rfd, NULL, NULL, &tv);

	FD_ZERO(&rfd);
	FD_SET(fd1, &rfd);
	FD_ZERO(&wfd);
	FD_SET(fd1, &wfd);
	FD_ZERO(&efd);
	printf("single socket read/write check\n");
	test(fd1 + 1, &rfd, &wfd, &efd, &tv);



	fd2 = Xsocket(AF_XIA, SOCK_DGRAM, 0);
printf("socket 1 = %d\n", fd2);
	FD_ZERO(&rfd);
	FD_SET(fd1, &rfd);
	FD_SET(fd2, &rfd);
	FD_ZERO(&wfd);
	FD_SET(fd1, &wfd);
	FD_SET(fd2, &wfd);
	FD_ZERO(&efd);
	printf("multiple socket read/write check\n");
	test(fd2 + 1, &rfd, &wfd, &efd, &tv);

	fd3 = Xsocket(AF_XIA, SOCK_DGRAM, 0);
printf("socket 3 = %d\n", fd3);
	FD_ZERO(&rfd);
	FD_SET(fd1, &rfd);
	FD_SET(fd2, &rfd);
	FD_SET(fd3, &rfd);
	FD_ZERO(&wfd);
	FD_SET(fd1, &wfd);
	FD_SET(fd3, &wfd);
	FD_ZERO(&efd);
	printf("Mixed return\n");
	test(fd3 + 1, &rfd, &wfd, &efd, &tv);

	fd4 = Xsocket(AF_XIA, SOCK_STREAM, 0);
printf("socket 4 = %d\n", fd4);
	FD_ZERO(&rfd);
	FD_SET(fd1, &rfd);
	FD_SET(fd2, &rfd);
	FD_SET(fd3, &rfd);
	FD_SET(fd4, &rfd);
	FD_ZERO(&wfd);
	FD_SET(fd1, &wfd);
	FD_SET(fd3, &wfd);
	FD_SET(fd4, &wfd);
	FD_ZERO(&efd);
	FD_SET(fd4, &efd);
	printf("testing unconnected stream socket\n");
	test(fd4 + 1, &rfd, &wfd, &efd, &tv);
//
//	ufds[0] = ufds[4];
//	ufds[0].events = POLLIN|POLLOUT;
//	printf("testing single unconnected stream socket\n");
//	test(&ufds[0], 1, 2000);
//
	return 0;
}
