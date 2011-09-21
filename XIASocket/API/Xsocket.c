/* 
 * Creates an XIA socket. 
 * When called it creates a socket to connect to Click using UDP packets. 
 * It first opens a socket and listens on some random port. Nextit sends
 * an open request to Click, from that socket. It waits (blocking) for a 
 * reply from Click. The control info is encoded in the UDP headers.
 *
 * On success the return packet's UDP source port is the same as the 
 * destination port. On failure, source port is 0
 * 
 * Arguments:
 * char* sDAG : This is registered to allow reverse traffic
 * TODO: Add XID type
 *
 * Return values:
 * sockfd if successful
 * -1 on failure
 *
 */

#include "Xsocket.h"

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

int Xsocket()
{

	//Setup to listen for control info
    char* str="open";//TODO: Not necessary. Maybe more useful data could be sent in the open control packet?
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_in their_addr,sin;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	int sockfd,tries;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	//hints.ai_flags = AI_PASSIVE; // Listen only on the Tun interface IP

	//MYPORT=0 will choose a random port
	int port=atoi(MYPORT);
    char* sport=MYPORT;

	//Open a port and listen on it
	if ((rv =getaddrinfo(MYADDRESS, MYPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("Xsocket listener: socket");
			continue;
		}

		//If port is in use, try next port until success, or max ATTEMPTS reached
		rv=-1;
		for (tries=0;tries<ATTEMPTS&&rv==-1;tries++,port++)
		{
			rv=bind(sockfd, p->ai_addr, p->ai_addrlen);

		}
		port--;

		if (rv== -1) {
			close(sockfd);
			perror("Xsocket listener: bind");
			continue;
		}

		break;
	}
	
	//find local port
	int len = sizeof(sin);
	getsockname(sockfd,(struct sockaddr *)&sin,&len);

	//memset(&in,0,sizeof(in));
	//in.s_addr = sin.sin_addr.s_addr;

	port= ntohs(sin.sin_port);
	//close(sockfd);
	//return 0;

	if (p == NULL) {
		fprintf(stderr, "Xsocket listener: failed to bind socket\n");
		return -1;
	}

	freeaddrinfo(servinfo);
	//printf("Xsocket listener: Sending...\n");

	//Send a control packet

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(CLICKCONTROLADDRESS, CLICKOPENPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

    p=servinfo;
    
	if ((numbytes = sendto(sockfd, str, strlen(str), 0,
					p->ai_addr, p->ai_addrlen)) == -1) {
		perror("Xsocket(): sendto failed");
		return(-1);
	}
	freeaddrinfo(servinfo);
	
    //printf("Xsocket listener: Sent. waiting to recvfrom...\n");

   
	//Process the reply
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
					(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("Xsocket: recvfrom");
		return -1;
	}
	buf[numbytes] = '\0';

	//When Click was able to open a connection it returns the same port number 

	int src_port=ntohs(their_addr.sin_port);
	//printf("rx port:%d, tx port%d\n", src_port,port);


	if (src_port==port)
		return sockfd;
	close(sockfd);
	return -1;
}


