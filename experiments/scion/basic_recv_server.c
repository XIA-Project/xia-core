#include "Xsocket.h"
#include "dagaddr.hpp"
#include <stdio.h>

#define XION_SID "SID:0123fff000000000000000000000000000000001"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <attaching rounter AD number>\n");
    return -1;
  }
  int adnum = atoi(argv[1]);
  if (adnum == 0) {
    fprintf(stderr, "invalid ad number\n");
    return -1;
  } else {
    switch (adnum) {
      case 2:
        set_conf("xsockconf.ini", "ad2host");
        break;
      case 3:
        set_conf("xsockconf.ini", "ad3host");
        break;
      default:
        fprintf(stderr, "invalid ad number\n");
        return -1;
    }
  }
  fprintf(stderr, "attached to AD %d\n", adnum);

  // socket
  int sock;
  if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Xsocket error\n");
    return -1;
  }

  // addrinfo
  struct addrinfo *ai;
  if (Xgetaddrinfo(NULL, XION_SID, NULL, &ai) != 0) {
    fprintf(stderr, "Xgetaddrinfo error\n");
    return -1;
  }
  sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
  Graph g((sockaddr_x*)ai->ai_addr);
  printf("My DAG: %s\n", g.dag_string().c_str());

  // bind
  if (Xbind(sock, (sockaddr *)sa, sizeof(sa)) < 0) {
    fprintf(stderr, "Xbind error\n");
    return -1;
  }

  while (true) {
    int n;
    char buf[1024];
    sockaddr_x client_dag;
    socklen_t dlen;

    if ((n = Xrecvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_dag, &dlen)) < 0) {
      fprintf(stderr, "Xrecvfrom error\n");
      break;
    }

    buf[n] = '\0';

    printf("recved %d bytes, DATA: \"%s\"\n", n, buf);
  }

  return 0;
}
