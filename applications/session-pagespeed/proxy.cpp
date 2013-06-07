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

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("usage: %s [-p port] \n", name);
	printf("where:\n");
	printf(" -p port   : port for incoming browser connections (default is 8888)\n");
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

	while ((c = getopt(argc, argv, "hl:s:")) != -1) {
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
			default:
				help(basename(argv[0]));
		}
	}
}



void process(int sock) {
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
		}
		string request(recvbuf);
		LOGF("%s", request.c_str());

		/* hacky way of getting hostname */
		vector<string> elements = split(request, '\n');
		string host;
		vector<string>::iterator it;
		for (it = elements.begin(); it != elements.end(); ++it) {
			if ((*it).find("Host:") != (*it).npos) {
				host = trim(split((*it), ' ')[1]);
				break;
			}
		}
		
		LOGF("Hostname: %s", host.c_str());
	
		/* Initiate session */
		ctx = SnewContext();
		if (ctx < 0) {
			ERROR("Error creating new context"); exit(-1);
		}
		if (Sinit(ctx, host.c_str(), "client", "client") < 0 ) {
		//if (Sinit(ctx, "www.test.cmu.edu", "client", "client") < 0 ) {
			ERROR("Error initiating session");
			exit(-1);
		}

		/* send file request */
		if (Ssend(ctx, recvbuf, bytesReceived) < 0) {
			ERROR("Error sending request");
			exit(-1);
		}

		/* as we receive data, pipe it to browser. a bit confusing perhaps,
		   but we're re-using recvbuf */
//int total = 0;
//int pktcount = 0;
		bytesReceived = Srecv(ctx, recvbuf, BUFSIZE);
		while (bytesReceived > 0) {
//total += bytesReceived;
//pktcount++;
//LOGF("Received %d bytes, cumm total: %d; packet # %d", bytesReceived, total, pktcount);
			send(sock, recvbuf, bytesReceived, 0);
			bytesReceived = Srecv(ctx, recvbuf, BUFSIZE);
		}  // TODO: We'll never get out!






		if (request.find("Connection: keep-alive") == request.npos) {
			break;
		}
	}

	LOGF("Closing socket %d", sock);
    close(sock);
}

void signal_callback_handler(int signum)
{
	LOG("Signal handler");
	close(listensock);
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

