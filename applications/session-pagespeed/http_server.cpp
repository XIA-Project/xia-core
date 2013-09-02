#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <stdio.h>
#include <sys/stat.h>
#include "session.h"
#include "utils.hpp"
#include "http.hpp"

using namespace std;

#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: INFO  %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: INFO  " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif
#define ERROR(s) fprintf(stderr, "\033[0;31m%s:%d: ERROR  %s\n\033[0m", __FILE__, __LINE__, s)
#define ERRORF(fmt, ...) fprintf(stderr, "\033[0;31m%s:%d: ERROR  " fmt"\n\033[0m", __FILE__, __LINE__, __VA_ARGS__) 


#define MAXBUF 4096

int process(int ctx)
{
	char buf[MAXBUF];
	char *method;
	char *fullpath;
	const char *path;
	char *protocol;
	struct stat statbuf;

	/* Receive request */
	memset(&buf[0], 0, sizeof(buf));
	if (SrecvADU(ctx, buf, MAXBUF) < 0) {
	    printf("Error receiving data\n");
		return -1;
	}

	LOGF("Ctx: %d    Request:\n%s", ctx, buf);

	method = strtok(buf, " ");
	fullpath = strtok(NULL, " ");
	protocol = strtok(NULL, "\r");

	// TODO: make this better
	// get rid of the hostname in the path
	string pathStr = "./www/";
	string tempStr = string(fullpath);
	size_t found = tempStr.find("/");
	found = tempStr.find("/", found+1);
	found = tempStr.find("/", found+1);
	pathStr += tempStr.erase(0, found+1);
	path = pathStr.c_str();
	LOGF("Ctx %d    Path: %s", ctx, path);

	if (!method || !path || !protocol) return -1;


	if (strcasecmp(method, "GET") != 0)
		send_error(ctx, 501, "Not supported", NULL, "Method is not supported.");
	else if (stat(path, &statbuf) < 0)
		send_error(ctx, 404, "Not Found", NULL, "File not found.");
	else if (S_ISDIR(statbuf.st_mode))
	{
		ERROR("Directories not supported");
	}
	else
		send_file(ctx, path, &statbuf);

	return 0;
}


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

    int listen_ctx, accept_ctx;

    // Make "listen" context
	listen_ctx = SnewContext();
	if (listen_ctx < 0) {
		ERROR("Error creating new context");
		exit(-1);
	}

    // Bind to name "server"
	if ( Sbind(listen_ctx, "www.test.cmu.edu") < 0 ) {
		ERROR("Error binding");
		exit(-1);
	}

    while (1) {
    
		LOG("Listening...");
    	accept_ctx = SacceptConnReq(listen_ctx);
		if (accept_ctx < 0) {
			ERROR("Error accepting connection request");
			exit(-1);
		}
    	LOG("Accepted a new connection");
    	
    	pid_t pid = fork();
    
    	if (pid == 0) {  
    		// child
    		process(accept_ctx);
			break;
    	}
    }
    return 0;
}
