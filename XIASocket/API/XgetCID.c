/*
* GetCID request
*/

#include "Xsocket.h"
#include "Xinit.h"

int XgetCID(int sockfd, char* dDAG, size_t dlen)
{
    const char *buf="CID request";//Maybe send more useful information here
    return Xsendto(sockfd,buf,strlen(buf)+1,0,dDAG,dlen);
}

