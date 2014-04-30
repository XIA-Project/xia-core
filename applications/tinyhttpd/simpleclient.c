#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include "xia.h"
#include "dagaddr.hpp" // only needed to pretty print the dag
#define SID "SID:8080808080808080808080808080808080808080"

int sockfd;

void watchdog(int sig) 
{
  if (sig == SIGALRM){
    close(sockfd);
    printf("Timeout! \n");
    exit(-1);
  }
}


int main(int argc, char *argv[])
{

 int len;
 struct addrinfo *ai;
 int result;
 //char ch = 'A';
 char buf[10000];
 struct timeval start, end;
 long mtime, seconds, useconds; 
 gettimeofday(&start, NULL);

 sockfd = socket(AF_XIA, SOCK_STREAM, 0);
 if (getaddrinfo(NULL, SID, NULL, &ai) < 0) {
	 printf("getaddrinfo faliure\n");
	 return 0;
 }

 Graph g((sockaddr_x*)ai->ai_addr);
 printf("%s\n", g.dag_string().c_str());

 signal(SIGALRM, watchdog);
 alarm(5);

 len = sizeof(sockaddr_x);
 result = connect(sockfd, ai->ai_addr, len);

 if (result == -1)
 {
  perror("oops: client1");
  exit(1);
 }
 write(sockfd, "GET / HTTP/1.0\n\n", 16);

 // FIXME: didn't add code to detect end of file
 int cnt = 1;
 while ( (cnt = read(sockfd, buf, sizeof(buf))) > 0 ) {
	buf[cnt] = 0;
	//printf("%s\n", buf);
	if(strstr(buf, "</HTML>") != NULL) {
		break;
	}
 }
 gettimeofday(&end, NULL);
 seconds  = end.tv_sec  - start.tv_sec;
 useconds = end.tv_usec - start.tv_usec;
 mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
 printf("Elapsed time: %ld milliseconds\n", mtime);
 close(sockfd);
 exit(0);
}
