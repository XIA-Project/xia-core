#include "Xsocket.h"

int Xaccept(int sockfd)
{
   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	
    //Send a control packet to inform Click of socket closing
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKACCEPTPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

    p=servinfo;
    char *buf="accept socket";
	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xaccept(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);

}

