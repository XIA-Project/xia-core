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

#ifdef __cplusplus
extern "C" {
#endif

#define ATTEMPTS 3 //Number of attempts at opening a socket 
#define MAXBUFLEN 2000

//Function list
extern int Xsendto(int sockfd,const void *buf, size_t len, int flags,char * dDAG, size_t dlen);
extern int Xrecvfrom(int sockfd,void *buf, size_t len, int flags,char * dDAG, size_t *dlen);
extern int Xsocket();
extern int Xconnect(int sockfd, char* dest_DAG);
extern int Xbind(int sockfd, char* SID);
extern int Xclose(int sock);
extern int Xrecv(int sockfd, void *buf, size_t len, int flags);
//extern int Xsend(int sockfd,const void *buf, size_t len, int flags);
extern int Xsend(int sockfd,void *buf, size_t len, int flags);
extern int XgetCID(int sockfd, char* dDAG, size_t dlen);
extern int XputCID(int sockfd, const void *buf, size_t len, int flags,char* sDAG, size_t dlen);
extern int Xaccept(int sockfd);
extern void error(const char *msg);
extern void set_conf(const char *filename, const char *sectioname);
extern void print_conf();
#ifdef __cplusplus
}
#endif

#endif

