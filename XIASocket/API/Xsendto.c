/*
* sendto like datagram sending function for XIA
*/

#include "Xsocket.h"

int Xsendto(int sockfd,char *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, socklen_t addrlen)
{

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	socklen_t addr_len;

	//TODO: Modify buf to add headers from dest_addr and addrlen. Change type from sockaddr to DAG 

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKDATAPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	numbytes = sendto(sockfd, buf, strlen(buf), flags, p->ai_addr, p->ai_addrlen);
	return numbytes;
}
