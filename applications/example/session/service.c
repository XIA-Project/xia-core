/*XIA Server. Does putCID, listens on SIDs etc*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xssocket.h"

#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define HID2 "HID:0000000000000000000000000000000000000002"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:1100000000000000000000000000000000000000"
#define RHID1 "HID:1100000000000000000000000000000000000001"
#define CID0 "CID:2000000000000000000000000000000000000000"
#define CID1 "CID:2000000000000000000000000000000000000001"
#define CID2 "CID:2000000000000000000000000000000000000002"
#define SID0 "SID:0f00000000000000000000000000000000000055"
#define SID2 "SID:0f00000000000000000000000000000000009022"


int main(int argc, char *argv[])
{
    int sock, chunkSock, n, acceptSock;
    size_t dlen;
    char buf[1024],theirDAG[1024];
    //char* reply="Got your message";
    char reply[1024];
    pid_t pid;

    //Open socket
    sock=Xssocket(XSOCK_STREAM);
    if (sock < 0) printf("error Opening socket");

    //Make the sDAG (the one the service listens on)
    char * dagstr = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD1, RHID1,SID2) + 1);
    sprintf(dagstr, "RE %s %s %s", AD1, RHID1,SID2); 
	Graph dag = Graph(dagstr);
	sockaddr_x *sa = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	dag.fill_sockaddr(sa);


    //Bind to the DAG
    Xsbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x));
    


    while (1) {
    
    	printf("\nListening on %s ...\n", dagstr);
    	acceptSock = Xsaccept(sock, SID2);
    	printf("Accept!\n");
    	
    	pid = fork();
    
    	if (pid == 0) {  
    		// child
    		
    		while (1) {
    			//Receive packet
			//n = Xrecvfrom(sock,buf,1024,0,theirDAG,&dlen);
			memset(&buf[0], 0, sizeof(buf));
			
			n = Xsrecv(acceptSock,buf,1024,0);
			
			if (n < 0) 
			    printf("error recvfrom");
			//printf("\nReceived a datagram from:%s len %d strlen %d\n",theirDAG, (int)dlen, (int)strlen(theirDAG));
			write(1,buf,n);


			// Do processing on received data here...
			// Change message to upper case
			int i;
			for (i = 0; i < n; i++) {
				if (buf[i] >= 97 && buf[i] <= 122)
					buf[i] -= 32;
				else if (buf[i] >= 65 && buf[i] <= 90)
					buf[i] += 32;
			}


			Xssend(acceptSock, buf, n, 0);

    		}
    	}

    }
    return 0;
}

