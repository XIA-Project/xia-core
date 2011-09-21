/* UDP client in the internet domain */
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

void error(const char *);

int main(int argc, char *argv[])
{
	int sock, n;

	//Open socket
	sock=Xsocket();
	if (sock < 0) error("Opening socket");
	
	//XBind is optional
	//Xbind(sock,"RE AD:1000000000000000000000000000000000000009 HID:1500000000000000000000000000000000000055 SID:1f00000000000000000000000000000000000055");
	write(1,"\nConnecting...\n",15);
	Xconnect(sock,"RE AD:0000000000000000000000000000000000000009 HID:0500000000000000000000000000000000000055 SID:0f00000000000000000000000000000000000055");
	char buffer[2048];
	printf("Please enter the message: ");
	bzero(buffer,2048);
	fgets(buffer,2048,stdin);
	
	Xsend(sock,buffer,strlen(buffer),0);
	write(1,"\nSent.\n",7);
	Xclose(sock);
	return 0;
}

