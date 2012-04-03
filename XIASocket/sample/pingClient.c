/*XIA Ping client*/

//TODO: Run the ping sender and ping receiver on different threads, so that Xrecv is running all the time, but send is scheduled

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "Xsocket.h"
//#include <fcntl.h>

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

void error(const char *);

int main(int argc, char *argv[]) {
    int sock, n,i,seq_client_=0,seq_c=0,seq_s=0,rx=0;
    size_t dlen;
    char reply[1024];
    char payload_new[2048],theirDAG[1024];    
    struct timeval tv;
    uint64_t current_time;
    FILE *fp;

    fp=fopen("output_client","w");
    if (fp==NULL) error("Error opening output file"); 

    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    print_conf();
    if (sock < 0) 
	error("Opening socket");

    //Make it non-blocking
//    fcntl(sock, F_SETFL, O_NONBLOCK);

    //XBind is optional. If not done an ephemeral port will be bound 
    //Xbind(sock,"RE AD:1000000000000000000000000000000000000009 HID:1500000000000000000000000000000000000055 SID:1f00000000000000000000000000000000000055");

    //Make the dDAG (the one you want to send packets to)
    char * dag = (char*)malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
    sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);

    
    for (i=0;i<TOTALPINGS;i++) {
	//Use Xconnect() with Xsend()
	gettimeofday(&tv, NULL);
	current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	memcpy (payload_new, &seq_client_, 4);

	Xsendto(sock, payload_new,4,0,dag,strlen(dag));

	fprintf(fp, "%llu: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload
	//printf("%lld: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload         
	seq_client_++;

	dlen = sizeof(theirDAG);
    n = Xrecvfrom(sock,reply,1024,0,theirDAG,&dlen);
	if(n>0)
	    {   
		gettimeofday(&tv, NULL);
		current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
		memcpy (&seq_c,reply, 4);
		memcpy (&seq_s,reply+4, 4);   
		fprintf(fp, "%llu: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
		//printf("%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
		rx++;
		n=0;
	    }   
	    //Sleep for x microseconds between pings
	    usleep(100);

    }

//    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) & ~O_NONBLOCK);
    if(rx<TOTALPINGS)  
	do
	{   
		dlen = sizeof(theirDAG);
	    n = Xrecvfrom(sock,reply,1024,0,theirDAG,&dlen);
	    gettimeofday(&tv, NULL);
	    current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	    memcpy (&seq_c,reply, 4);
	    memcpy (&seq_s,reply+4, 4);   
	    fprintf(fp, "%llu: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
	    //printf("%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
	    rx++;
	    n=0;
	}
	while(rx<TOTALPINGS)   ;

    fclose(fp);
    Xclose(sock);
    return 0;
}

