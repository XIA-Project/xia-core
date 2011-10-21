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
        struct Netinfo info;

        //Open socket
        xnet_sockid=Xsocket();
        if (xnet_sockid < 0) error("Opening socket");

        num_active_sockets = Xgetsocketidlist(xnet_sockid, socket_list);

        int i;

        printf("Proto\t SrcDAG\t DstDAG\t Status \n");
        for(i= 0; i<num_active_sockets; i++) { 

                Xgetsocketinfo(xnet_sockid, socket_list[i], &info);
                printf("%s\t %s\t %s\t %s \n", info.protocol, info.src_path, info.dst_path, info.status);
        }


        Xclose(xnet_sockid);

        return 1;
}


