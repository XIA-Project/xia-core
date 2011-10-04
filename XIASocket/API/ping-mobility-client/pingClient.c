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
#include <fcntl.h>

#define HID0 "HID:0000000000000000000000000000000000000000"
#define HID1 "HID:0000000000000000000000000000000000000001"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define AD1   "AD:1000000000000000000000000000000000000001"
#define AD2   "AD:1000000000000000000000000000000000000002"
#define RHID0 "HID:0000000000000000000000000000000000000002"
#define RHID1 "HID:0000000000000000000000000000000000000003"
#define CID0 "CID:2000000000000000000000000000000000000001"
#define SID0 "SID:0f00000000000000000000000000000000000055"

//Change in ping server as well
#define TOTALPINGS 4000
#define MIGRATEPOINT 2000

//Play with the below values to get 1000pps. 10 and 10 are quite close
#define RECVLOOPCOUNT 11
#define RECVLOOPWAIT 10

#define ENDTIMEOUT 15 //in seconds

#define DEBUG 0

void error(const char *);

int main(int argc, char *argv[])
{
	int sock, n,dlen,i,seq_client_=0,seq_c,seq_s,rx=0;
	char reply[1024];
	char payload_new[2048],theirDAG[1024];    
	struct timeval tv;
	uint64_t current_time, start_time,end_time;
    FILE *fp;
    
    fp=fopen("output_client","w");
    
	//Open socket
	sock=Xsocket();
	if (sock < 0) 
		error("Opening socket");
		
    //Make it non-blocking
    fcntl(sock, F_SETFL, O_NONBLOCK);

	//XBind is optional. If not done an ephemeral port will be bound 
	//Xbind(sock,"RE AD:1000000000000000000000000000000000000009 HID:1500000000000000000000000000000000000055 SID:1f00000000000000000000000000000000000055");

	//Make the dDAG (the one you want to send packets to)
	char * dag = malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, HID0,SID0) + 1);
	sprintf(dag, "RE %s %s %s", AD0, HID0,SID0);

	//Use connect if you want to use Xsend instead of Xsendto
	//printf("\nConnecting...\n");
	Xconnect(sock,dag);//Use with Xrecv
	
	//To stop after 15 secs
    gettimeofday(&tv, NULL);
    start_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
    

	for (i=0;i<MIGRATEPOINT;i++)
	{
		//Use Xconnect() with Xsend()
		gettimeofday(&tv, NULL);
        current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
		memcpy (payload_new, &seq_client_, 4);
		Xsend(sock,payload_new,4,0);
        fprintf(fp, "%lld: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload
        if (DEBUG)
            printf("%lld: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload         
        seq_client_++;
                 
		//Or use Xsendto()
		//Xsendto(sock,payload_new,4,0,dag,strlen(dag));
		//printf("Sent\n");

		//Process reply from server
		//n = Xrecvfrom(sock,reply,1024,0,theirDAG,&dlen);
		int j=0;
		for(j=0;j<RECVLOOPCOUNT;j++)
		{
    		n = Xrecv(sock,reply,1024,0);
    		while(n>0)
    	    {   
    	        gettimeofday(&tv, NULL);
                current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
    	        memcpy (&seq_c,reply, 4);
    	        memcpy (&seq_s,reply+4, 4);   
    	        fprintf(fp, "%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
    	        if (DEBUG)
      	            printf("%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
    	        rx++;
    	        n=0;
    	        n = Xrecv(sock,reply,1024,0);
    	    }   
    	    //Sleep for x microseconds between pings
    		usleep(RECVLOOPWAIT);
    	}

	}
	
	//Run the mobility script
	system("echo 'write move.run' | nc 127.0.0.1 7777");
	gettimeofday(&tv, NULL);
	current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	fprintf(fp, "%lld: updating XIAPingSource with new address something something\n",current_time);  // modify payload
    printf("\nMoved!\n");
    
	for (i=MIGRATEPOINT;i<TOTALPINGS;i++)
	{
		//Use Xconnect() with Xsend()
		gettimeofday(&tv, NULL);
        current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
		memcpy (payload_new, &seq_client_, 4);
		Xsend(sock,payload_new,4,0);
        fprintf(fp, "%lld: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload
        if (DEBUG)
            printf("%lld: PING sent; client seq = %d\n",current_time, seq_client_);  // modify payload         
        seq_client_++;
                 
		//Or use Xsendto()
		//Xsendto(sock,payload_new,4,0,dag,strlen(dag));
		//printf("Sent\n");

		//Process reply from server
		//n = Xrecvfrom(sock,reply,1024,0,theirDAG,&dlen);
		int j=0;
		for(j=0;j<RECVLOOPCOUNT;j++)
		{
    		n = Xrecv(sock,reply,1024,0);
    		while(n>0)
    	    {   
    	        gettimeofday(&tv, NULL);
                current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
    	        memcpy (&seq_c,reply, 4);
    	        memcpy (&seq_s,reply+4, 4);   
    	        fprintf(fp, "%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
    	        if (DEBUG)
    	            printf("%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
    	        rx++;
    	        n=0;
    	        n = Xrecv(sock,reply,1024,0);
    	    }   
    	    //Sleep for x microseconds between pings
    		usleep(RECVLOOPWAIT);
    	}

	}
	gettimeofday(&tv, NULL);
    end_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) & ~O_NONBLOCK);
    if(rx<TOTALPINGS)  
   	do
	{   
	    n = Xrecv(sock,reply,1024,0);
	    gettimeofday(&tv, NULL);
        current_time = (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
	    if(n>0)
	    {
        memcpy (&seq_c,reply, 4);
        memcpy (&seq_s,reply+4, 4);   
        fprintf(fp, "%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
        if (DEBUG)    
            printf("%lld: PONG received; client seq = %d, server seq = %d\n",current_time, seq_c, seq_s);
        rx++;
        n=0;
        }
        
        if(seq_c==TOTALPINGS-1)
            break;
        if(seq_c>TOTALPINGS-3)
        {
            float diff=(float)(current_time-start_time)/1000000;
            //printf("diff=%f\n",diff);
            if(diff>ENDTIMEOUT)
                break; 
            usleep(RECVLOOPWAIT);
        }
    }
    while(rx<TOTALPINGS);
    printf("\nPings per second=%f\nPings sent=%d\nPings received=%d\nPings lost=%d\n",(float)TOTALPINGS/((float)(end_time-start_time)/1000000),TOTALPINGS,rx,TOTALPINGS-rx);
    fclose(fp);
	Xclose(sock);
	return 0;
}

