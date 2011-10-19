/*Xnetstat*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xsocket.h"

#define MAXSOCKETS 1000 // maximum number of sockets 

int main(int argc, char *argv[])
{
	int xnet_sockid;  // socket for this Xnetstat
	int num_active_sockets;
	int socket_list[MAXSOCKETS];
        struct DAGinfo info;

	//Open socket
	xnet_sockid=Xsocket();
	if (xnet_sockid < 0) error("Opening socket");
		
	num_active_sockets = Xgetsocketidlist(xnet_sockid, socket_list);

	int i;
	for(i= 0; i<num_active_sockets; i++) { 

		//printf("sockid=%d \n", socket_list[i]);
		Xgetsocketinfo(xnet_sockid, socket_list[i], &info);
		printf("port=%d \n", info.port);
	}

	//print(Xgetsocketinfo(sockid));

	Xclose(xnet_sockid);
	
	return 1;
}

