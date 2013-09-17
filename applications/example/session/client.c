#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "session.h"

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif

#define MAXBUF 4096
#define RECV_TIMEOUT_MICRO 750000
#define CHECK_INTERVAL_MICRO 100000

// global configuration options
int loops = -1;		// only do 1 pass
int pktSize = 1024;	// default pkt size

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("usage: %s [-l loops] [-s size]\n", name);
	printf("where:\n");
	printf(" -l loops   : loop <loops> times and exit (0 loops infinitely; -1 prompts for user input)\n");
	printf(" -s size    : set packet size to <size>. if 0, uses random sizes (default is %d bytes)\n", pktSize);
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
			case 'l':
				// loop <loops> times and exit
				// if 0, loop forever
				loops = atoi(optarg);
				if (loops < 0) loops = 0;
				break;
			case 's':
				// send pacets of size <size> maximum is 1024
				// if 0, send random sized packets
				pktSize = atoi(optarg);
				if (pktSize < 0) pktSize = 0;
				if (pktSize > MAXBUF) pktSize = MAXBUF;
				break;
			default:
				help(basename(argv[0]));
		}
	}
}

void randomString(char *buf, int size)
{
	int i;
	static const char *filler = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static int refresh = 1;
	int samples = strlen(filler);

	if (!(--refresh)) {
		// refresh rand every now and then so it doesn't degenerate too much
		//  use a prime number to keep it interesting
		srand(time(NULL));
		refresh = 997;
	}
	for (i = 0; i < size - 1; i ++) {
		buf[i] = filler[rand() % samples];
	}
	buf[size - 1] = '\0';
}


int main(int argc, char *argv[])
{
	srand(time(NULL));
	getConfig(argc, argv);

    int ctx, n;
    char reply[MAXBUF];
    char buffer[MAXBUF];
    
	// Initiate session
	ctx = SnewContext();
	if (ctx < 0) {
		LOG("Error creating new context");
		exit(-1);
	}
	if (Sinit(ctx, "service, server", kReliableDelivery) < 0 ) {
		LOG("Error initiating session");
		exit(-1);
	}

    int count = 0; 
    while(1)
    {
		int size;
		bzero(buffer, MAXBUF);
		if (loops == -1) {
			printf("\nPlease enter the message (0 to exit): ");
			if (fgets(buffer,MAXBUF,stdin) == NULL) {
				LOG("Error reading user input");
			}
			if (buffer[0]=='0'&&strlen(buffer)==2)
			    break;
		} else {
			if (pktSize == 0)
				size = (rand() % MAXBUF) + 1;
			else
				size = pktSize;
			randomString(buffer, size);
		}
		    
		Ssend(ctx, buffer, strlen(buffer));

		
		// Wait a little bit for data to become available
		long int waited = 0;
		while (!ScheckForData(ctx) && waited < RECV_TIMEOUT_MICRO) {
			usleep(CHECK_INTERVAL_MICRO);
			waited += CHECK_INTERVAL_MICRO;
		}

		if (ScheckForData(ctx)) { // don't hang
			n = SrecvADU(ctx, reply, sizeof(reply));
			if (n < 0) 
			    printf("Error receiving data");
			if (write(1,reply,n) < 0) {
				printf("Error writing reply");
			}
			printf("\n");
		}

		count++;
		if (loops > 0 && count == loops)
			break;
    }

	Sclose(ctx);
    return 0;
}

