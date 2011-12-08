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

#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000000"
#define CID1 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"


int main(int argc, char *argv[])
{
    int sock, n, acceptSock;
    size_t dlen;
    char buf[1024],theirDAG[1024];
    //char* reply="Got your message";
    char reply[1024];
    pid_t pid;

    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    if (sock < 0) error("Opening socket");

    //print_conf(); //for debugging purpose
    
    //Make a CID entry
    char * cdag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID0) + 1);
    sprintf(cdag, "RE %s %s %s", AD0, HID0,CID0); 
    char* data="Some value stored for CID0";
    //XputCID(sock,data,strlen(data),0,cdag,strlen(cdag));


    //Make the sDAG (the one the server listens on)
    char * dag = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0); 
    //printf("\nListening on RE %s %s %s", AD0, HID0,SID0);


    //Bind to the DAG
    Xbind(sock,dag);

    while (1) {
    			//Receive packet
    			memset(&buf[0], 0, sizeof(buf));
			n = Xrecvfrom(sock,buf,1024,0,theirDAG,&dlen);
			
			//n = Xrecv(acceptSock,buf,1024,0);
			
			if (n < 0) 
			    error("recvfrom");
			//printf("\nReceived a datagram from:%s len %d strlen %d\n",theirDAG, (int)dlen, (int)strlen(theirDAG));
			write(1,buf,n);
			
			memset(&reply[0], 0, sizeof(reply));
			strcat (reply, "Got your message: ");
			strcat (reply, buf);			

			//Reply to client
			Xsendto(sock,reply,strlen(reply)+1,0,theirDAG,strlen(theirDAG));
	
			//Xsend(acceptSock,reply,strlen(reply),0);
   

    }
    return 0;
}

