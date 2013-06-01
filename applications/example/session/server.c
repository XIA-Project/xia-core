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

#define MAXBUF 2048


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

    int n, listen_ctx, accept_ctx;
    char buf[MAXBUF];
    char reply[MAXBUF];
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
    		
    		while (1) {
    			// Receive packet
				memset(&buf[0], 0, sizeof(buf));
				n = SrecvADU(accept_ctx, buf, MAXBUF);
				
				if (n < 0) {
				    printf("Error receiving data\n");
					return -1;
				}
				if (write(1,buf,n) < 0) {
					printf("Error writing to buf\n");
				}

				//Reply to client
				memset(&reply[0], 0, sizeof(reply));
				strcat (reply, buf);
				
				if (Ssend(accept_ctx, reply, strlen(reply)) < 0) {
					LOG("Error sending reply");
				}
    		}
    	}
    }
    return 0;
}
