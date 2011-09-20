#include "Xsocket.h"

int Xclose(int sockfd)
{
   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	
    //Send a control packet to inform Click of socket closing
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKCLOSEPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

    p=servinfo;
    char *buf="close socket";
	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xclose(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);
    close(sockfd);
}

