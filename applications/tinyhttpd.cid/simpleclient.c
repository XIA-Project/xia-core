#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdlib.h>
#include <netdb.h>
#include "xia.h"
#include "dagaddr.hpp" // only needed to pretty print the dag
#define SID "SID:8080808080808080808080808080808080808080"

int main()
{
 int sockfd;
 int len;
 struct addrinfo *ai;
 int result;
 char buf[10000];

 sockfd = socket(AF_XIA, SOCK_STREAM, 0);
 if (getaddrinfo(NULL, SID, NULL, &ai) < 0) {
	 printf("getaddrinfo faliure\n");
	 return 0;
 }

 Graph g((sockaddr_x*)ai->ai_addr);
 printf("%s\n", g.dag_string().c_str());

 len = sizeof(sockaddr_x);
 result = connect(sockfd, ai->ai_addr, len);

 if (result == -1)
 {
  perror("oops: client1");
  exit(1);
 }
 send(sockfd, "GET / HTTP/1.0\n\n", 16, 0);

 // FIXME: didn't add code to detect end of file
 while (1) {
	int cnt = recv(sockfd, buf, sizeof(buf), 0);
	buf[cnt] = 0;
	printf("%s\n", buf);
 }

 close(sockfd);
 exit(0);
}
