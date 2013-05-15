#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
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
    int n, listen_ctx, accept_ctx;
    size_t dlen;
    char buf[1024],theirDAG[1024];
    char reply[1024];
    pid_t pid;

    // Make "listen" context
	listen_ctx = SnewContext();
	if (listen_ctx < 0) {
		LOG("Error creating new context");
		exit(-1);
	}

    // Bind to name "server"
	if ( Sbind(listen_ctx, "server") < 0 ) {
		LOG("Error binding");
		exit(-1);
	}

    while (1) {
    
		LOG("Listening...");
    	accept_ctx = SacceptConnReq(listen_ctx);
		if (accept_ctx < 0) {
			LOG("Error accepting connection request");
			exit(-1);
		}
    	LOG("Accepted a new connection");
    	
    	pid = fork();
    
    	if (pid == 0) {  
    		// child
    		
    		/*while (1) {
    			// Receive packet
				memset(&buf[0], 0, sizeof(buf));
				n = Xsrecv(acceptSock,buf,1024,0);
				
				if (n < 0) 
				    printf("error recvfrom");
				//printf("\nReceived a datagram from:%s len %d strlen %d\n",theirDAG, (int)dlen, (int)strlen(theirDAG));
				write(1,buf,n);

				//Reply to client
				//Xsendto(sock,reply,strlen(reply)+1,0,theirDAG,strlen(theirDAG));
				
				memset(&reply[0], 0, sizeof(reply));
				strcat (reply, "Got your message: ");
				strcat (reply, buf);
				
				Xssend(acceptSock,reply,strlen(reply),0);
    		}*/
    	}
    }
    return 0;
}
