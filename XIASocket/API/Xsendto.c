/*
* sendto like datagram sending function for XIA
*/

#include "Xsocket.h"
#include "Xinit.h"

int Xsendto(int sockfd,const void *buf, size_t len, int flags,
		char* dDAG, size_t dlen)
{

    /*
     * This is an ugly way to do things. This essentially concatenates the dDAG and buf
     * using a ^ to demarcate the two. As long as the DAG contains no ^ it should be fine.
     * Ideally you want to pass some data structure with the required information, but 
     * since we need to use strings and UDP packets, this will do for now.
     */
    
    char*s=(char*)malloc(MAXBUFLEN);
    strcpy(s,dDAG);
    int i=strlen(dDAG);
    s[i]='^';
    int offset=i+1;
    memcpy(s+offset, buf, len);
    
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKSENDTOPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	numbytes = sendto(sockfd, s, len+i+1, flags, p->ai_addr, p->ai_addrlen);
	return numbytes;
}
