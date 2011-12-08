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

void error(const char *);

int main(int argc, char *argv[])
{
    int sock, n;
    size_t dlen;
    char reply[128];
    char buffer[2048],theirDAG[1024];    
    
    struct cDAGvec cDAGv[3];

    //Open socket
    sock=Xsocket(XSOCK_STREAM);
    if (sock < 0) 
	error("Opening socket");

    //XBind is optional. If not done an ephemeral port will be bound 
    //Xbind(sock,"RE AD:1000000000000000000000000000000000000009 HID:1500000000000000000000000000000000000055 SID:1f00000000000000000000000000000000000055");

    //Make the dDAG (the one you want to send packets to)
    char * dag = malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);

    //Use connect if you want to use Xsend instead of Xsendto
    printf("\nTry connecting...\n");
    Xconnect(sock,dag);//Use with Xrecv
    printf("\nConnected.\n");

    //Try getCID for CID0, CID1, and CID2
    char * cdag0 = malloc(snprintf(NULL, 0, "RE ( %s %s ) %s", AD0, HID0,CID0) + 1);
    sprintf(cdag0, "RE ( %s %s ) %s", AD0, HID0,CID0); 
    char * cdag1 = malloc(snprintf(NULL, 0, "RE ( %s %s ) %s", AD0, HID0,CID1) + 1);
    sprintf(cdag1, "RE ( %s %s ) %s", AD0, HID0,CID1); 
    char * cdag2 = malloc(snprintf(NULL, 0, "RE ( %s %s ) %s", AD0, HID0,CID2) + 1);
    sprintf(cdag2, "RE ( %s %s ) %s", AD0, HID0,CID2);
    
    cDAGv[0].cDAG = cdag0;
    cDAGv[0].dlen = strlen(cdag0);
    cDAGv[1].cDAG = cdag1;
    cDAGv[1].dlen = strlen(cdag1);
    cDAGv[2].cDAG = cdag2;
    cDAGv[2].dlen = strlen(cdag2);

     
    //printf("cDAGList=%s \n", cdagList);
    XgetCIDList(sock, cDAGv, 3);
    
    //printf("Hello \n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);
    
    printf("\n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);
    
    printf("\n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);

    //Try the same getCID again (for debugging purposes)
    XgetCIDList(sock, cDAGv, 3);
    printf("\n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);
    printf("\n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);
    
    printf("\n");
    n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
    if (n < 0) 
	error("recvfrom");
    write(1,reply,n);

    while(1)
    {
	printf("\nPlease enter the message (0 to exit): ");
	bzero(buffer,2048);
	fgets(buffer,2048,stdin);
	if (buffer[0]=='0'&&strlen(buffer)==2)
	    break;
	    
	//Use Xconnect() with Xsend()
	Xsend(sock,buffer,strlen(buffer),0);
	
	//Or use Xsendto()

	//Xsendto(sock,buffer,strlen(buffer),0,dag,strlen(dag)+1);
	printf("Sent\n");


	//Process reply from server
	//n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
	n = Xrecv(sock,reply,128,0);
	if (n < 0) 
	    error("recvfrom");
	//printf("Received a datagram from:%s\n",theirDAG);
	write(1,reply,n);
	printf("\n");
    }

    Xclose(sock);
    return 0;
}

