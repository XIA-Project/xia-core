/*
* GetCID request
*/

#include "Xsocket.h"

int XgetCID(int sockfd, char* dDAG, size_t dlen)
{
    char *buf="CID request";//Maybe send more useful information here
    return Xsendto(sockfd,buf,strlen(buf),0,dDAG,dlen);
}

