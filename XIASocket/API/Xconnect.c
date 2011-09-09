#include "Xsocket.h"


int Xconnect(int sockfd, char* dest_DAG)
{
	//Setup to listen for control info

   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	
    //Send a control packet to inform Click of socket closing
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKBINDPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

    p=servinfo;
    
	if ((numbytes = sendto(sockfd, dest_DAG, strlen(dest_DAG), 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xbind(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);

	return 0;
}

