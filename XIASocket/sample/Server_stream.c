/*XIA Server. Does putCID, listens on SIDs etc*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xsocket.h"

#define SID0 "SID:0f00000000000000000000000000000000001777"

#define SNAME "www_s.stream_echo.aaa.xia"

int main(int argc, char *argv[])
{
    int sock, chunkSock, n, acceptSock;
    size_t dlen;
    char buf[1024],theirDAG[1024];
    //char* reply="Got your message";
    char reply[1024];
    pid_t pid;
    char myAD[1024]; 
    char myHID[1024];    
    char my4ID[1024]; 

    //Open socket
    sock=Xsocket(XSOCK_STREAM);
    if (sock < 0) error("Opening socket");
 
    // read the localhost AD and HID
    if ( XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0 )
    	error("Reading localhost address");

    // make the src DAG (the one the server listens on)
    char * dag = (char*) malloc(snprintf(NULL, 0, "RE ( %s ) %s %s %s", my4ID, myAD, myHID, SID0) + 1);
    sprintf(dag, "RE ( %s ) %s %s %s", my4ID, myAD, myHID, SID0);  

    //Register this service name to the name server
    char * sname = (char*) malloc(snprintf(NULL, 0, "%s", SNAME) + 1);
    sprintf(sname, "%s", SNAME);      
    if (XregisterName(sname, dag) < 0 )
    	error("name register");

    //Bind to the DAG
    Xbind(sock,dag);
    

    while (1) {
    
    	printf("\nListening...\n");
    	acceptSock = Xaccept(sock);
    	printf("Accept!\n");
    	
    	pid = fork();
    
    	if (pid == 0) {  
    		// child
    		
    		while (1) {
    			//Receive packet
			//n = Xrecvfrom(sock,buf,1024,0,theirDAG,&dlen);
			memset(&buf[0], 0, sizeof(buf));
			
			n = Xrecv(acceptSock,buf,1024,0);
			
			if (n < 0) 
			    error("recvfrom");
			//printf("\nReceived a datagram from:%s len %d strlen %d\n",theirDAG, (int)dlen, (int)strlen(theirDAG));
			write(1,buf,n);

			//Reply to client
			//Xsendto(sock,reply,strlen(reply)+1,0,theirDAG,strlen(theirDAG));
			
			memset(&reply[0], 0, sizeof(reply));
			strcat (reply, "Got your message: ");
			strcat (reply, buf);
			
			Xsend(acceptSock,reply,strlen(reply),0);
    		
    		}
    	}

    }
    return 0;
}

