#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <libgen.h>
#endif
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#define MAX_XID_SIZE 100
#define NAME "www_s.multihong_server.aaa.xia"


/*
char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}
*/
int verbose = 1;	// display all messages
/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

}

/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "exiting\n");
	exit(ecode);
}

void *server(void *socketid){
	int sock = *((int *)socketid);
	char buf[XIA_MAXBUF];
	sockaddr_x cdag;
	socklen_t dlen;
	int n;
	
	say("Datagram service started\n");


	pid_t pid = 0;

	say("Dgram Server waiting\n");

	dlen = sizeof(cdag);
	memset(buf, 0, sizeof(buf));
	/*if (n = Xrecv(sock, buf, sizeof(buf), 0) < 0) {
		warn("Recv error on socket %d, closing connection\n", pid);
		break;
	}*/
	int count = 0;
	n=0;
	while ((count = Xrecv(sock, buf, sizeof(buf), 0)) != 0) {
		say("%5d received %d bytes\n", pid, count);
		n += count;
		int c = 0;
		if ((c = Xsend(sock, buf, count, 0)) < 0) {
			warn("%5d send error\n", pid);
			break;
		}

		say("%5d sent %d bytes\n", pid, c);
	}
	say("server received %d bytes\n", n);
	
	/*if ((n = Xsend(sock, buf, n, 0)) < 0) {
		warn("%5d send error\n", pid);
		break;
	}*/

	say("server sent %d bytes\n", n);

	Xclose(sock);
}

int registerReceiver()
{
	int sock;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	//say ("\n%s (%s)\n", TITLE, VERSION);
	//say("Service Name: %s\n", name);
	//say("Root Directory: %s\n", rootdir);
	//say("Max Chunk Size: %u\n\n", chunksize);

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}
	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	
	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	if (XregisterName(NAME, dag) < 0 )
		die(-1, "error registering name: %s\n", NAME);

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	Xlisten(sock, 5);

	Graph g(dag);
	say("listening on dag: %s\n", g.dag_string().c_str());

	return sock;
}

void *blockingListener(void *socketid)
{
	int sock = *((int*)socketid);
	int acceptSock;
	while (1) {
		say("Waiting for a client connection\n");

		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");

		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, server, (void *)&acceptSock);
	}

	Xclose(sock); // we should never reach here!
	return NULL;
}

int main()
{
	int sock = registerReceiver();
	blockingListener((void *)&sock);
	
	return 0;
}
