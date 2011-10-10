/*
* send like datagram sending function for XIA
* IMPORTANT: Works for datagrams only
*/

#include "Xsocket.h"
#include "Xinit.h"

int Xsend(int sockfd,void *buf, size_t len, int flags)
{

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKDATAPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	numbytes = sendto(sockfd, buf, len, flags, p->ai_addr, p->ai_addrlen);
	return strlen((char*)buf);
}
