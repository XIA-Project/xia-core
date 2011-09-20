/* Creates a datagram server.  The port 
 *    number is passed as an argument.  This
 *       server runs forever */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xsocket.h"


int main(int argc, char *argv[])
{
	int sock, length, n;
	char buf[1024];
/*
	if (argc < 3) {
		fprintf(stderr, "ERROR, provide MYADDRESS and CLICKCONTROLADDRESS\n");
		exit(0);
	}
	
	//Set some variables
	strcpy(MYADDRESS,argv[1]);
	strcpy(CLICKCONTROLADDRESS,argv[2]);
*/
	sock=Xsocket();
	if (sock < 0) error("Opening socket");
	
	Xbind(sock,"RE AD:0000000000000000000000000000000000000009 HID:0500000000000000000000000000000000000055 SID:0f00000000000000000000000000000000000055");
    write(1,"\nListening...\n",14);
    while (1) {
		n = Xrecv(sock,buf,1024,0);
		if (n < 0) error("recvfrom");
		//printf("Received a datagram of len:%d\n",n);
		write(1,buf,n);
	}
	return 0;
}

