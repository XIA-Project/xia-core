/*
* sendto like datagram sending function for XIA
*/

#include "Xsocket.h"

int XputCID(int sockfd,void *buf, size_t len, int flags,
		char* sDAG, size_t dlen)
{

    /*
     * This is an ugly way to do things. This essentially concatenates the sDAG and buf
     * using a ^ to demarcate the two. As long as the DAG contains no ^ it should be fine.
     * Ideally you want to pass some data structure with the required information, but 
     * since we need to use strings and UDP packets, this will do for now.
     */
    
    //Should probably check if the sDAG ends with a CID
    
    char * s = malloc(snprintf(NULL, 0, "%s^%s",sDAG, (char*)buf) + 1);
    sprintf(s, "%s^%s", sDAG,  (char*)buf);

    
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	socklen_t addr_len;
	

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;


	if ((rv = getaddrinfo(CLICKDATAADDRESS, CLICKPUTCIDPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	p=servinfo;

	numbytes = sendto(sockfd, s, strlen(s), flags, p->ai_addr, p->ai_addrlen);
	return numbytes;
}
