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
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"

void error(const char *);

int main(int argc, char *argv[])
{
	int sock, n;
	size_t dlen;
	char reply[128];
	char buffer[2048],theirDAG[1024];    

	//Open socket
	sock=Xsocket();
	if (sock < 0) 
		error("Opening socket");

	//XBind is optional. If not done an ephemeral port will be bound 
	//Xbind(sock,"RE AD:1000000000000000000000000000000000000009 HID:1500000000000000000000000000000000000055 SID:1f00000000000000000000000000000000000055");

	//Make the dDAG (the one you want to send packets to)
	char * dag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
	sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);

	//Use connect if you want to use Xsend instead of Xsendto
	//printf("\nConnecting...\n");
	//Xconnect(sock,dag);//Use with Xrecv

	//Try a getCID
	char * cdag = (char*)malloc(snprintf(NULL, 0, "RE ( %s %s ) %s", AD0, HID0,CID0) + 1);
	sprintf(cdag, "RE ( %s %s ) %s", AD0, HID0,CID0); 
	XgetCID(sock,cdag,strlen(cdag));
	n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
	if (n < 0) 
		error("recvfrom");
	write(1,reply,n);

	//Try the same getCID again (for debugging purposes)
	XgetCID(sock,cdag,strlen(cdag));
	n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
	if (n < 0) 
		error("recvfrom");
	printf("\n");
	write(1,reply,n);


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

		Xsendto(sock,buffer,strlen(buffer),0,dag,strlen(dag));
		printf("Sent\n");


		//Process reply from server
		n = Xrecvfrom(sock,reply,128,0,theirDAG,&dlen);
		if (n < 0) 
			error("recvfrom");
		//printf("Received a datagram from:%s\n",theirDAG);
		write(1,reply,n);
		printf("\n");
	}

	Xclose(sock);
	return 0;
}

