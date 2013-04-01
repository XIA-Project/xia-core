#include "Xsocket.h"
#include "dagaddr.hpp"

#include <string>

#define XION "XION:00505d002008088010000000000000009cad0000000000009cad0000000000008098555e5101000208140000000ca6fa2000000a003125828073555e510100022d00000b003125820c1e000000c2a0277768617420746865206675636b"
#define XION_UNRESOLV "XION_UNRESOLV:0000000000000000000000000000000000000003"
#define AD  "AD:1000000000000000000000000000000000000001"
#define HID "HID:0000000000000000000000000000000000000003"
#define BHID "HID:1111111111111111111111111111111111111111"
#define SID "SID:0123fff000000000000000000000000000000001"

typedef struct {
  uint16_t target_if;
  uint16_t payload_len;
  uint8_t payload[];
} scion_control_packet;

int main(int argc, char *argv[]) {
  set_conf("xsockconf.ini", "ad2host");
  if (argc < 2) {
    fprintf(stderr, "usage: %s [data]\n", argv[0]);
    return -1;
  }

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
//  Graph g = Node() * Node(AD)  * Node(HID) * Node(SID);
//  std::string xion_node_str = std::string(XION2);
//  Graph g = Node() * Node::XIONNode(XION) * Node(HID) * Node(SID);
  Graph g = Node() * Node(XION_UNRESOLV) * Node(HID) * Node(SID);
  g.fill_sockaddr(&ddag);
  fprintf(stderr, "%s\n", g.dag_string().c_str());

  char *payload = argv[1];

  int n;
  if ((n = Xsendto(sock, payload, strlen(payload), 0, (sockaddr *)&ddag, sizeof(sockaddr_x))) < 0) {
    fprintf(stderr, "Xsendto error\n");
    return -1;
  }
}
