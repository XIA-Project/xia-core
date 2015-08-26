#include <iostream>
#include "chunks.h"
#include "Xsocket.h"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

//Just registering the service and openning the necessary sockets
int registerReceiver(void)
{
	int sock;

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		return -1;

	// read the localhost AD and HID
	if (XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0)
		return -1;

	struct addrinfo *ai;
	//FIXME: SID is hardcoded
	if (Xgetaddrinfo(NULL, CHUNKS_SID, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	//FIXME NAME is hard coded
	if (XregisterName(CHUNKS_NAME, dag) < 0 )
		return -1;

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		return -1;
	}

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

int main(void)
{
	int sock = registerReceiver();
	blockingListener((void *)&sock);
	return 0;
}
