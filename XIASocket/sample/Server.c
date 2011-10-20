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
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"


int main(int argc, char *argv[])
{
    int sock, dlen, n;
    char buf[1024],theirDAG[1024];
    char* reply="Got your message";

    //Open socket
    sock=Xsocket();
    if (sock < 0) error("Opening socket");


    //Make a CID entry
    char * cdag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID0) + 1);
    sprintf(cdag, "RE %s %s %s", AD0, HID0,CID0); 
    char* data="Some value stored for CID0";
    XputCID(sock,data,strlen(data),0,cdag,strlen(cdag));


    //Make the sDAG (the one the server listens on)
    char * dag = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0); 
    //printf("\nListening on RE %s %s %s", AD0, HID0,SID0);


    //Bind to the DAG
    Xbind(sock,dag);
    printf("\nListening...\n");

    while (1) {
	//Receive packet
	//n = Xrecvfrom(sock,buf,1024,0,theirDAG,&dlen);
	n = Xrecv(sock,buf,1024,0);
	if (n < 0) 
	    error("recvfrom");
	printf("Received a datagram from:%s\n",theirDAG);
	write(1,buf,n);

	//Reply to client
	//Xsendto(sock,reply,strlen(reply),0,theirDAG,strlen(theirDAG));
	Xsend(sock,reply,strlen(reply),0);

    }
    return 0;
}

