#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>
#ifdef __APPLE__
#include <libgen.h>
#endif
#include <unistd.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include "Xkeys.h"
#include "Xsocket.h"
#include "stage_utils.h"

#define NAME "www_s.multihong_server.aaa.xia"
#define MAX_BUF_SIZE 62000
#define MAX_XID_SIZE 100

//int verbose = 1;	// display all messages
int delay = 100000;		// don't delay between loops
int loops = 3000;		// only do 1 pass
int pktSize = 512;	// default pkt size
int reconnect = 0;	// don't reconnect between loops
int threads = 1;	// just a single thread
int terminate = 0;  // sighandler sets to 1 if we should quit

struct addrinfo *ai;

/*
** write the message to stdout unless in quiet mode
*/
/*
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}
*/
/*
** always write the message to stdout
*/
/*
void warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}
*/
/*
** write the message to stdout, and exit the app
*/
/*
void die(int ecode, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "exiting\n");
	exit(ecode);
}
*/
/*
** create a semi-random alphanumeric string of the specified size
*/
char *randomString(char *buf, int size)
{
	int i;
	static const char *filler = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static int refresh = 1;
	int samples = strlen(filler);

	if (!(--refresh)) {
		// refresh rand every now and then so it doesn't degenerate too much
		// use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = 0;

	return buf;
}

/*
** do one send/receive operation on the specified socket
*/
int process(int sock)
{
	int size;
	int sent = 0;
	int received = 0;
	char buf1[MAX_BUF_SIZE + 1], buf2[MAX_BUF_SIZE + 1];

	if (pktSize == 0)
		size = (rand() % MAX_BUF_SIZE) + 1;
	else
		size = pktSize;
	randomString(buf1, size);
	printf("%d\n", (int)size);
	int count = size;

	sent = Xsend(sock, buf1, count, 0);
	if (sent < 0)
	{
		die(-4, "Send error %d on socket %d\n", errno, sock);
	}

	say("Xsock %4d sent %d of %d bytes\n", sock, sent, size);

	int rc;
	struct pollfd pfds[2];
	pfds[0].fd = sock;
	pfds[0].events = POLLIN;
	if ((rc = Xpoll(pfds, 1, 50000)) <= 0) {
		die(-5, "Poll returned %d\n", rc);
	}

	memset(buf2, 0, sizeof(buf2));
	count = 0;
	while (sent != received && (count = Xrecv(sock, &buf2[received], sizeof(buf2) - received, 0)) > 0) {
		say("%5d received %d bytes\n", sock, count);
		received += count;
		buf2[received] = 0;
	}

	say("Xsock %4d received %d bytes in total\n", sock, received);

	if (sent != received || strcmp(buf1, buf2) != 0)
		warn("Xsock %4d received data different from sent data! (bytes sent/recv'd: %d/%d)\n",
				sock, sent, received);

	return 0;
}

/*
** do a short pause between operations
*/
void pausex()
{
	int t;
	if (delay == -1)
		// default - don't pause at all
		return;
	else if (delay == 0)
		// pause for some random period less than a second
		t = rand() % 1000000;
	else
		// pause for the specfied number of hundredths of a second
		t = delay;
	usleep(t);
}

/*
** create a socket and connect to the remote server
*/
int flag = 0;
int connectToServer(struct ifaddrs *ifa)
{
	int ssock;
	if ((ssock = Xsocket(AF_XIA, XSOCK_STREAM, 0)) < 0) {
		die(-2, "unable to create the server socket\n");
	}
	say("Xsock %4d created\n", ssock);
	
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}
	
	struct addrinfo hints, *ai;
	sockaddr_x *sa;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	hints.ai_flags |= XAI_DAGHOST;
	
	Graph g((sockaddr_x *) ifa->ifa_addr);
	//while(1){
		Xgetaddrinfo(g.dag_string().c_str(), sid_string, &hints, &ai);  // hints should have the XAI_DAGHOST flag set
		//Use the sockaddr_x returned by Xgetaddrinfo in your next call
		sa = (sockaddr_x*)ai->ai_addr;
		//Graph gg(sa);
		//printf("\n%s\n", gg.dag_string().c_str());
		//usleep(1 * 1000);
	//}
	
	Xbind(ssock, (struct sockaddr*)sa, sizeof(sa));
	Graph gg(sa);
	printf("\n%s\n", gg.dag_string().c_str());
	
	sockaddr_x dag;
	socklen_t daglen = sizeof(dag);
	
	//if (Xgetaddrinfo(MUL_SERVER1, NULL, NULL, &sai) != 0)
	//	die(-1, "unable to lookup name %s\n", MUL_SERVER1);
	if (XgetDAGbyName(NAME, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", NAME);
	
	if (Xconnect(ssock, (struct sockaddr *)&dag, sizeof(sockaddr_x)) < 0)
		die(-3, "unable to connect to the destination dag\n");
	
	char sdag[5000];
	char ip[100];
	bzero(sdag,sizeof(sdag));
	
	say("Xsock %4d connected\n", ssock);

	return ssock;
}

/*
** the main loop for talking to the remote server
** when threading is enabled, one of these will run in each thread
**
** The parameter and return code are there to satisify the thread library
*/
void *mainLoop(void * ifaddr)
{
	struct ifaddrs *ifa = (struct ifaddrs *)ifaddr;
	int ssock;
	int count = 0;
	int printcount = 1;
	
	if (loops == 1)
		printcount = 0;

	//struct sockaddr * sa = ifa->ifa_addr;
	ssock = connectToServer(ifa);
	
	while (true) {		
		say("Xsock %4d loop #%d\n", ssock, count);
		if (process(ssock) != 0)
			break;
		pausex();
		count++;
		if (loops > 0 && count == loops)
				break;
	}
	Xclose(ssock);
	say("Xsock %4d closed\n", ssock);
	return NULL;
}

/*
** where it all happens
*/
int main(int argc, char **argv)
{
	srand(time(NULL));
	//getConfig(argc, argv);
  
	struct ifaddrs*if1=NULL, *if2=NULL;
	
	struct ifaddrs *ifa=NULL;
	struct ifaddrs *ifaddr = NULL;
	
	if( Xgetifaddrs(&ifaddr) < 0){
		die(-1, "Xgetifaddrs failed");
	}
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {  
        if (ifa->ifa_addr == NULL)  
            continue;  
        printf("interface: %s \n", ifa->ifa_name); 
		if(strcmp(ifa->ifa_name, "iface0")==0) if1 = ifa;//"wlp6s0"
		if(strcmp(ifa->ifa_name, "iface1")==0 ) if2 = ifa;//"wlx60a44ceca928"
    } 
	
		
		
	pthread_t *clients = (pthread_t*)malloc(2 * sizeof(pthread_t));

	if (!clients)
		die(-5, "Unable to allocate threads\n");

	pthread_create(&clients[0], NULL, mainLoop, (void *)if1);
	pthread_create(&clients[1], NULL, mainLoop, (void *)if2);
	
	for (int i = 0; i < 2; i++) {
		pthread_join(clients[i], NULL);
	}

	free(clients);
	
	return 0;
}
