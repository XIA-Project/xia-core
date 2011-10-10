#include "Xsocket.h"
#include "Xinit.h"


int Xconnect(int sockfd, char* dest_DAG)
{

   	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	
    //Send a control packet to inform Click of connect request
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKCONNECTPORT, &hints, &servinfo)) != 0) {
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

