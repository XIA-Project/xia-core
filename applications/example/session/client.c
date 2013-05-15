#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "session.h"

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif


int main(int argc, char *argv[])
{
    int ctx, n;
    size_t dlen;
    char reply[128];
    char buffer[2048];
    
	// Initiate session
	ctx = SnewContext();
	if (ctx < 0) {
		LOG("Error creating new context");
		exit(-1);
	}
	if (Sinit(ctx, "server", "client", "client") < 0 ) {
		LOG("Error initiating session");
		exit(-1);
	}





/*    //Open socket
    sock=Xssocket(XSOCK_STREAM);
    if (sock < 0) 
		printf("error Opening socket");

	// Make the DAGs: path1 = path from client to server; path2 = path from server to client
	char* path1str = (char*)malloc(snprintf(NULL, 0, "DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s 5 3 - \n %s 5 4 - \n %s 5 - \n %s", AD1, RHID1, SID2, AD0, HID0, SID0)+1);
	sprintf(path1str, "DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s 5 3 - \n %s 5 4 - \n %s 5 - \n %s", AD1, RHID1, SID2, AD0, HID0, SID0);
	Graph path1 = Graph(path1str);

	char* path2str = (char*)malloc(snprintf(NULL, 0, "DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s", AD1, HID1, "SID:1f00000000000000000000000000000000000055")+1);
	sprintf(path2str, "DAG 2 0 - \n %s 2 1 - \n %s 2 - \n %s", AD1, HID1, "SID:1f00000000000000000000000000000000000055");
	Graph path2 = Graph(path2str);

	//XBind is optional. If not done an ephemeral port will be bound 
    //Xsbind(sock, path2);

    //Use connect if you want to use Xsend instead of Xsendto
    printf("\nTry connecting...\n");
    Xsconnect(sock, path1.dag_string().c_str(), path2.dag_string().c_str(), path2.dag_string().c_str());//Use with Xrecv
    printf("\nConnected.\n");

     
    while(1)
    {
	printf("\nPlease enter the message (0 to exit): ");
	bzero(buffer,2048);
	fgets(buffer,2048,stdin);
	if (buffer[0]=='0'&&strlen(buffer)==2)
	    break;
	    
	//Use Xconnect() with Xsend()
	Xssend(sock,buffer,strlen(buffer),0);
	
	//Or use Xsendto()

	//Xsendto(sock,buffer,strlen(buffer),0,dag,strlen(dag)+1);
	printf("Sent \n");


	//Process reply from server
	n = Xsrecv(sock,reply,128,0);
	if (n < 0) 
	    printf("error recvfrom");
	//printf("Received a datagram from:%s\n",theirDAG);
	write(1,reply,n);
	printf("\n");
    }

    Xsclose(sock); */
    return 0;
}

