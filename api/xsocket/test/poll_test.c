

#include <sys/poll.h>	
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "Xsocket.h"
#include "../Xinit.h"

int test(struct pollfd *ufds, unsigned nfds, int timeout)
{
	int e;
	printf("ufds:%p nfds:%d timeout:%d\n", ufds, nfds, timeout);

	time_t start = time(NULL);
	errno = 0;
	int rc = Xpoll(ufds, nfds, timeout);
	e = errno;
	time_t end = time(NULL);

	printf("rc:%d errno:%d, elapsed:%d\n", rc, e, (int)(end - start));

	if (rc > 0) {
		for (int i = 0; i < rc; i++)
			printf("sock:%d flags:%x\n", ufds[i].fd, ufds[i]. revents);
	}
	printf("\n");

	return rc;
}

int main()
{
	struct pollfd ufds[10];

	get_conf();

	printf("bad parameters\n");
	test(NULL, 1, 10000);

	ufds[0].fd = 999;
	ufds[0].events = POLLIN|POLLOUT;
	printf("invalid socket (out flags = 0x20)\n");
	test(ufds, 1, 1000);

	ufds[0].fd = -10;
	ufds[0].events = POLLIN;
	printf("single socket no-op check\n");
	test(ufds, 1, 1000);

	printf("just a timeout\n");
	test(ufds, 0, 1000);

	printf("Xsock + stdin (press return or wait 3 seconds)\n");
	ufds[0].fd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	ufds[0].events = POLLIN;
	ufds[1].fd = 2;
	ufds[1].events = POLLIN;
	printf("socket 0 = %d\n", ufds[0].fd);
	test(ufds, 2, 3000);


	printf("single socket read check\n");
	test(ufds, 1, 1000);

	ufds[0].events = POLLIN|POLLOUT;
	printf("single socket read/write check\n");
	test(ufds, 1, 1000);

	ufds[1].fd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	printf("socket 1 = %d\n", ufds[1].fd);
	ufds[1].events = POLLIN|POLLOUT;
	printf("multiple socket read/write check\n");
	test(ufds, 2, 1000);

	ufds[2].fd = -3;
	ufds[2].events = 0;

	ufds[3].fd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	printf("socket 3 = %d\n", ufds[3].fd);
	ufds[3].events = POLLIN;
	printf("Mixed return\n");
	test(ufds, 4, 1000);

	ufds[4].fd = Xsocket(AF_XIA, SOCK_STREAM, 0);
	printf("socket 4 = %d\n", ufds[4].fd);
	ufds[4].events = POLLOUT;
	printf("testing unconnected stream socket\n");
	test(&ufds[4], 1, 1000);

	ufds[0] = ufds[4];
	ufds[0].events = POLLIN|POLLOUT;
	printf("testing single unconnected stream socket\n");
	test(&ufds[0], 1, 2000);

	return 0;
}
