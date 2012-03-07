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
#define CID2 "CID:2000000000000000000000000000000000000002"
#define SID0 "SID:0f00000000000000000000000000000000000055"


int main(int argc, char *argv[])
{
    int sock, chunkSock, n, acceptSock;
    size_t dlen;
    char buf[1024],theirDAG[1024];
    //char* reply="Got your message";
    char reply[1024];
    pid_t pid;

    //Open socket
    sock=Xsocket(XSOCK_STREAM);
    if (sock < 0) error("Opening socket");

    chunkSock=Xsocket(XSOCK_CHUNK);
    if (chunkSock < 0) error("Opening socket");

    //Make a CID entry
    char * cdag0 = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID0) + 1);
    sprintf(cdag0, "RE %s %s %s", AD0, HID0,CID0); 
    char* data0="Some value stored for CID0";
    XputCID(chunkSock,data0,strlen(data0),0,cdag0,strlen(cdag0));
    
    //Make another CID entry
    char * cdag1 = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID1) + 1);
    sprintf(cdag1, "RE %s %s %s", AD0, HID0,CID1); 
    char* data1="Other value stored for CID1";
    XputCID(chunkSock,data1,strlen(data1),0,cdag1,strlen(cdag1));    

    //Make another CID entry
    char * cdag2 = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,CID2) + 1);
    sprintf(cdag2, "RE %s %s %s", AD0, HID0,CID2); 
    char* data2="Other value stored for CID2";
    XputCID(chunkSock,data2,strlen(data2),0,cdag2,strlen(cdag2));

    //Make the sDAG (the one the server listens on)
    char * dag = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0); 
    //printf("\nListening on RE %s %s %s", AD0, HID0,SID0);


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

