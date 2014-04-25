#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include <stdlib.h>
#include <string.h>
#include <netdb.h>

int main(int argc, char *argv[])
{
 int sockfd;
 int len;
 struct sockaddr_in address;
 int result;
 //char ch = 'A';
  char buf[10000];
 struct timeval start, end;
 long mtime, seconds, useconds; 
 gettimeofday(&start, NULL);

 sockfd = socket(AF_INET, SOCK_STREAM, 0);
 address.sin_family = AF_INET;
 address.sin_addr.s_addr = inet_addr("127.0.0.1");
 address.sin_port = htons(9734);
 len = sizeof(address);
 result = connect(sockfd, (struct sockaddr *)&address, len);

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
