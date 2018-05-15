#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "xcache.h"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>
#include "chunk-demo.h"
#include "Xkeys.h"

int verbose = 1;

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
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}


int make_random_chunk(XcacheHandle *xcache, sockaddr_x *addr)
{
	char chunk[CHUNKSIZE];
	int i;

	for (i = 0; i < CHUNKSIZE; i++)
		chunk[i] = (char)random();

	if (XputChunk(xcache, (const char *)chunk, (size_t)CHUNKSIZE, addr) < 0)
		return -1;

	return 0;
}

void *recvCmd(void *socketid)
{
	int n;
	sockaddr_x addr;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	XcacheHandle xcache;

	XcacheHandleInit(&xcache);
	//ChunkContext contains size, ttl, policy, and contextID which for now is PID

	while (1) {
		say("waiting for command\n");

		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		} else if (n == 0) {
			warn("Peer closed the connection\n");
 			break;
		}
		//Sender does the chunking and then should start the sending commands
		if (strncmp(command, "make", 4) == 0) {
			say("Making a random CID chunk\n");
			if (make_random_chunk(&xcache, &addr) < 0) {
				die(-1, "Could not make random chunk\n");
			}
			Graph g(&addr);
			int len = g.http_url_string().size()+1;

			// Return the addr
			if (Xsend(sock, g.http_url_string().c_str(), len, 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}


	Xclose(sock);
	XcacheHandleDestroy(&xcache);
	pthread_exit(NULL);
}


//Just registering the service and openning the necessary sockets
int registerReceiver()
{
	int sock;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	say ("\n%s (%s): started\n", TITLE, VERSION);

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	struct addrinfo *ai;

	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	//FIXME NAME is hard coded
	if (XregisterName(NAME, dag) < 0 )
		die(-1, "error registering name: %s\n", NAME);

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
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
		pthread_create(&client, NULL, recvCmd, (void *)&acceptSock);
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
