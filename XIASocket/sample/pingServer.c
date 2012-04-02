/* XIA ping server*/
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "Xsocket.h"

#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"

#define TOTALPINGS 500
#define MIGRATEPOINT 2000


int main(int argc, char *argv[])
{
    int sock, n, seq_c,seq_s;
    size_t dlen;
    char payload_new[1024],theirDAG[1024];
    char* reply[1024];
    struct timeval tv;
    uint64_t current_time;
    FILE *fp;

    fp=fopen("output_server","w");
    if (fp==NULL) {
	error("Error opening output file");
    }

    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    print_conf(); /* For Debugging configuartion */
    if (sock < 0) error("Opening socket");

    //Make the sDAG (the one the server listens on)
    char * dag = malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);

    //Bind to the DAG
    Xbind(sock,dag);
    //printf("\nListening...\n");
    //Xaccept(sock);

    seq_s = 0;

    while (seq_s<TOTALPINGS) {
	//Receive packet
        dlen=sizeof(theirDAG);
	n = Xrecvfrom(sock,payload_new,1024,0,theirDAG,&dlen);

	gettimeofday(&tv, NULL);
	current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	
	if(n>0)
	{
	    memcpy (&seq_c,payload_new, 4);

	    memcpy (payload_new, &seq_c, 4);
	    memcpy (payload_new+4, &seq_s, 4);
	    gettimeofday(&tv, NULL);
	    current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	    if(seq_c==MIGRATEPOINT)
		fprintf(fp, "%llu: updating XIAPingResponder with new address something something\n",current_time);  // modify payload
	    fprintf(fp, "%llu: PING received; client seq = %d\n",current_time, seq_c);
	    //printf("%lld: PING received; client seq = %d\n",current_time, seq_c);

	    Xsendto(sock, payload_new,8,0,theirDAG,strlen(theirDAG));
	    gettimeofday(&tv, NULL);
	    current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	    fprintf(fp, "%llu: PONG sent; client seq = %d, server seq = %d\n",current_time,seq_c, seq_s);              
	    //printf("%lld: PONG sent; client seq = %d, server seq = %d\n",current_time,seq_c, seq_s);              

	    seq_s++;
	    n=0;
	}
    }
    fclose(fp);
    return 0;
}

