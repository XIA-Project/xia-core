#ifndef XSOCKET_H
#define XSOCKET_H

#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

//Socket library side: Control address/info
#define MYADDRESS "172.0.0.1" 
#define MYPORT "0"//Chooses random port

//Click side: Control/data address/info
//The actual IPs don't matter, it just has to be in the correct subnet
#define CLICKCONTROLADDRESS "172.0.0.2" 
#define CLICKOPENPORT "5001"
#define CLICKBINDPORT "5002"
#define CLICKCLOSEPORT "5003"
#define CLICKCONNECTPORT "5004"
#define CLICKCONTROLPORT "5005"


#define CLICKGETCIDPORT "5006"
#define CLICKDATAADDRESS "172.0.0.2" 
#define CLICKDATAPORT "10000"

//set xia.click sorter to sort based on these ports. 


#define ATTEMPTS 3 //Number of attempts at opening a socket 
#define MAXBUFLEN 2000

//Function list
extern int Xsendto(int sockfd,char *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
extern int Xrecvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);

extern int Xsocket();
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *buf, size_t len, int flags);
extern int Xsend(int sockfd,char *buf, size_t len, int flags);


#endif

