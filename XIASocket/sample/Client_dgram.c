/*XIA Client. Does getCID, connects to servers on SIDs etc*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "Xsocket.h"

#define SNAME "www_s.dgram_echo.aaa.xia"

void error(const char *);

int main(int argc, char *argv[])
{
    int sock, n;
    size_t dlen;
    char reply[128];
    char buffer[2048],theirDAG[1024];    

    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    if (sock < 0) 
	error("Opening socket");
	
    //Name query to the name server
    char * sname = (char*) malloc(snprintf(NULL, 0, "%s", SNAME) + 1);
    sprintf(sname, "%s", SNAME);      
    char * dag = XgetDAGbyName(sname);

    while(1)
    {
	printf("\nPlease enter the message (0 to exit): ");
	bzero(buffer,2048);
	fgets(buffer,2048,stdin);
	if (buffer[0]=='0'&&strlen(buffer)==2)
	    break;
	    
	//Use Xconnect() with Xsend()
	//Xsend(sock,buffer,strlen(buffer),0);
	
	//Or use Xsendto()

	Xsendto(sock,buffer,strlen(buffer),0,dag,strlen(dag)+1);
	printf("Sent\n");


	//Process reply from server
	dlen =  sizeof(theirDAG);
	n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
	//n = Xrecv(sock,reply,128,0);
	if (n < 0) 
	    error("recvfrom");
	//printf("Received a datagram from:%s\n",theirDAG);
	write(1,reply,n);
	printf("\n");
    }

    Xclose(sock);
    return 0;
}

