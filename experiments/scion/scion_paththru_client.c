#include "Xsocket.h"
#include "dagaddr.hpp"

#include <string>

#define SID "SID:0123fff000000000000000000000000000000001"

typedef struct {
  uint16_t target_if;
  uint16_t payload_len;
  uint8_t payload[];
} scion_control_packet;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <from ad> <target_ad> <data>\n", argv[0]);
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

  int sock;
  if ((sock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
    fprintf(stderr, "Xsocket error\n");
    return -1;
  }

  // addrinfo
  struct addrinfo *ai;
  if (Xgetaddrinfo(NULL, SID, NULL, &ai) != 0) {
    fprintf(stderr, "Xgetaddrinfo error\n");
    return -1;
  }

  // dag creation
  sockaddr_x ddag;
  char xion_unresolv_node[60];
  char xid_hid[60];
  int target_adnum = atoi(argv[2]);
  if (target_adnum == 0) {
    fprintf(stderr, "invalid ad number\n");
    return -1;
  }
  sprintf(xion_unresolv_node, "XION_UNRESOLV:%040d", target_adnum);
  sprintf(xid_hid, "HID:%040d", target_adnum);
  Graph g = Node() * Node(xion_unresolv_node) * Node(xid_hid) * Node(SID);
  g.fill_sockaddr(&ddag);
  fprintf(stderr, "%s\n", g.dag_string().c_str());

  char *payload = argv[3];

  int n;
  if ((n = Xsendto(sock, payload, strlen(payload), 0, (sockaddr *)&ddag, sizeof(sockaddr_x))) < 0) {
    fprintf(stderr, "Xsendto error\n");
    return -1;
  }
}
