/*
* recvfrom like datagram receiving function for XIA
* does not fill in DAG fields yet
*/

#include "Xsocket.h"

int Xrecvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen)
{
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int numbytes;
	socklen_t addr_len;
    char UDPbuf[MAXBUFLEN];
	struct sockaddr_in their_addr;

	//TODO: Modify buf to add headers from dest_addr and addrlen. Change type from sockaddr to DAG 

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	p=servinfo;

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("Xrecvfrom: recvfrom");
		return -1;
	}
	int src_port=ntohs(their_addr.sin_port);
	
	while(src_port==atoi(CLICKCONTROLPORT))
	{
    	//Do what is necessary, maybe close socket
    	if(strcmp(buf,"close")==0)
    	{
    	    Xclose(sockfd);
    	    return -1;
    	}
    	else
        {
    		if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) 
			{
	    	    perror("Xrecvfrom: recvfrom");
        		return -1;
           	}
	    
	    }
	}
	
	//TODO: Copy Xdata to buf, and headers to their src_addr
	//memcpy(buf, c+Xheader_size, numbytes-Xheader_size);
	memcpy(buf, UDPbuf, numbytes);
	
	return numbytes;
}
