#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <time.h>
#include <errno.h>
#include <signal.h>
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


#define MAX_CONNECTIONS 200
#define BUFSIZE 4096

int listensock, sock;

// global configuration options
int proxy_port = 8888;
bool scale_images = false;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("usage: %s [-p port] [-s]\n", name);
	printf("where:\n");
	printf(" -p port   : port for incoming browser connections (default is 8888)\n");
	printf(" -s        : use in-path service to scale down images (default is false)\n");
	printf("\n");
	exit(0);
}


/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;

	opterr = 0;

	while ((c = getopt(argc, argv, "hl:p:s")) != -1) {
		switch (c) {
			case '?':
			case 'h':
				// Help Me!
				help(basename(argv[0]));
				break;
			case 'p':
				// browser proxy port
				proxy_port = atoi(optarg);
				break;
			case 's':
				// scale images
				scale_images = true;
				break;
			default:
				help(basename(argv[0]));
		}
	}
}



void process(int sock) {
	LOGF("Serving browser on new socket: %d", sock);
	char recvbuf[BUFSIZE];
    int bytesReceived, ctx;

	while (true) {
    	/* Receive message from browser */
    	if ((bytesReceived = recv(sock, recvbuf, BUFSIZE, 0)) < 0) {
			close(sock);
			ERRORF("receive error: %s", strerror(errno));
			exit(-1);
		}
		if (bytesReceived >= BUFSIZE) {
			ERROR("We may not have received all of the message from the browser");
		} else if (bytesReceived == 0) {
			ERROR("Received empty message from browser");
			close(sock);
			return;  // TODO: figure out when browser closed socket
		}
		string request(recvbuf, bytesReceived);
		LOGF("New Request from browser:\n%s", request.c_str());
		/* make sure this is an HTTP GET */
		if (request.find("GET") == request.npos || request.find("HTTP") == request.npos) {
			ERROR("Message from browser was not an HTTP GET");
			continue;
		}

		/* get hostname */
		string host = hostNameFromHTTP(request);
		LOGF("Hostname: %s", host.c_str());
	
		/* Initiate session */
		ctx = SnewContext();
		if (ctx < 0) {
			ERROR("Error creating new context"); 
			break;
		}
		string sessionPath = scale_images ? host + ", pagespeed.cmu.edu" : host;
		if (Sinit(ctx, sessionPath.c_str()) < 0 ) {
			ERROR("Error initiating session");
			break;
		}

		/* send file request */
		if (Ssend(ctx, recvbuf, bytesReceived) < 0) {
			ERROR("Error sending request");
			break;
		}

		/* receive & forward the response HTTP header */
		if ( (bytesReceived = SrecvADU(ctx, recvbuf, BUFSIZE)) < 0 ) {
			ERROR("Error sending request");
			break;
		}
		string responseHeader(recvbuf, bytesReceived);
		send(sock, recvbuf, bytesReceived, 0);
		LOGF("Received response header:\n%s", responseHeader.c_str());

		/* receive & forward the # bytes specified by content length */
		int contentLength = contentLengthFromHTTP(responseHeader);
		int total = 0;
		while (total < contentLength) {
			if ( (bytesReceived = SrecvADU(ctx, recvbuf, BUFSIZE)) < 0) {
				ERROR("Error receiving");
			}
			send(sock, recvbuf, bytesReceived, 0);

			total += bytesReceived;
		} 


		/* if we're not supposed to keep the connection alive, exit */
		if (responseHeader.find("Connection: keep-alive") == request.npos) {
			LOGF("Ctx %d    No keep-alive found, closing connections", ctx);
			break;
		}
	}

	LOGF("Closing socket %d and context %d", sock, ctx);
    close(sock);
	Sclose(ctx);
}

void signal_callback_handler(int signum)
{
	LOG("Signal handler");
	close(listensock);
	close(sock);
	exit(signum);
}



int main(int argc, char *argv[])
{
	srand(time(NULL));
	getConfig(argc, argv);
	signal(SIGINT, signal_callback_handler);
	signal(SIGKILL, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);



	// Start listening for connections from browser
	if ((listensock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		ERRORF("error creating socket to listen on: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(proxy_port);

	if (bind(listensock, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(listensock);
		ERRORF("bind error: %s", strerror(errno));
		return -1;
	}

	if (listen(listensock, MAX_CONNECTIONS) < 0) {
		close(listensock);
		ERRORF("listen error: %s", strerror(errno));
		return -1;
	}

	while(true) {
		LOGF("Listening for a new connection on port %d", proxy_port);
		if ( (sock = accept(listensock, NULL, 0)) < 0) {
			close(sock);
			ERRORF("accept error: %s", strerror(errno));
			continue;
		}

		pid_t pid = fork();

		if (pid == -1) {
			close(sock);
			ERRORF("fork error: %s", strerror(errno));
			continue;
		} else if (pid == 0) { 
			process(sock);
			exit(0);
		} else {
			close(sock); // parent process doesn't need child socket
		}

	}


	close(listensock);
    return 0;
}

