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
#define MYADDRESS "127.0.0.1"
#define MYPORT "0"//Chooses random port

//Click side: Control/data address/info
//The actual IPs don't matter, it just has to be in the correct subnet
#define CLICKCONTROLADDRESS "127.0.0.1" 
#define CLICKOPENPORT "1"
#define CLICKBINDPORT "2"
#define CLICKCLOSEPORT "3"
#define CLICKCONNECTPORT "4"
#define CLICKCONTROLPORT "5"


#define CLICKGETCIDPORT "100"
#define CLICKDATAADDRESS "127.0.0.1" 
#define CLICKDATAPORT "1000"

//TODO: set xia.click sorter to sort based on these ports. Use all higher destination ports for data and set destination port=source port


#define ATTEMPTS 3 //Number of attempts at opening a socket 
#define MAXBUFLEN 2000

//Function list
extern int Xsendto(int sockfd,char *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
extern int Xrecvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);
extern int Xsocket(char* sDAG);
extern int Xclose(int sock);


#endif

