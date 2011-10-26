/*
 * recvfrom like datagram receiving function for XIA
 * does not fill in DAG fields yet
 */

#include "Xsocket.h"
#include "Xinit.h"

int Xrecvfrom(int sockfd, void *buf, size_t len, int flags,
	char* sDAG, size_t* slen)
{
    struct addrinfo hints;
    int numbytes;
    socklen_t addr_len;
    char UDPbuf[MAXBUFLEN];
    struct sockaddr_in their_addr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;


    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
		    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	perror("Xrecvfrom: recvfrom");
	return -1;
    }
    int src_port=ntohs(their_addr.sin_port);

    while(src_port==atoi(CLICKCONTROLPORT)) {
	//Do what is necessary, maybe close socket
	if(strcmp((char *)buf,"close")==0) {
	    Xclose(sockfd);
	    return -1;
	}
	else {
	    if ((numbytes = recvfrom(sockfd, UDPbuf, MAXBUFLEN-1 , flags,
			    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		//perror("Xrecvfrom: recvfrom");
		return -1;
	    }         		

	}	
    }


    //TODO: Copy Xdata to buf, and headers to their src_addr
    //memcpy(buf, c+Xheader_size, numbytes-Xheader_size);
    short int paylen=0,i=0;
    char* tmpbuf=(char*)UDPbuf;
    while(tmpbuf[i]!='^')
	i++;
    paylen=numbytes-i-1;
    //memcpy (&paylen, UDPbuf+2,2);
    //paylen=ntohs(paylen);
    int offset=i+1;
    memcpy(buf, UDPbuf+offset, paylen);
    strncpy(sDAG, UDPbuf, i);
    sDAG[i]=0; /* Make DAG a null terminated string */
    *slen=i;

    return paylen;
}
