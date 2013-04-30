#include "Xsocket.h"
#include "dagaddr.hpp"

#include <string>

#define XION "XION:005050002008088010000000000000009cad0000000000009cad00000000000080c5797f5101000208140000000ca6fa2000000a0031258280c3797f510100022d00000b003125820c1e000000c2a027"
#define HID "HID:0000000000000000000000000000000000000003"
#define BHID "HID:1111111111111111111111111111111111111111"
#define SID "SID:0123fff000000000000000000000000000000001"

typedef struct {
  uint16_t target_if;
  uint16_t payload_len;
  uint8_t payload[];
} scion_control_packet;

int read_scion_path(int fromad, int targetad, char *buf) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr=inet_addr("127.0.0.1");
  addr.sin_port=htons(7777);
  connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  char req[4096];
  char res_tmp[4096];
  char res[4096];
  sprintf(req, "read ad%dxionrouter/xion_bridge.request_scion_path %d\n", fromad, targetad);
  write(sock, req, strlen(req));
  if (read(sock, res_tmp, 4096) > 0) {
    sprintf(res, "%s%s", res, res_tmp);
  }
  char *res_ptr;
  if ((res_ptr = strstr(res, "DATA")) == NULL) {
    return -1;
  }
  res_ptr = strstr(res_ptr, "\n") + 1;
  strcpy(buf, res_ptr);
  return 0;
}

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

  int target_adnum = atoi(argv[2]);
  if (target_adnum == 0) {
    fprintf(stderr, "invalid ad number\n");
    return -1;
  }

  char scion_path[4096];
  if (read_scion_path(adnum, target_adnum, scion_path) == -1) {
    fprintf(stderr, "failed to get scion path for target ad %d from ad %d\n", adnum, target_adnum);
    return -1;
  }
  fprintf(stderr, "SCION_PATH: %s\n", scion_path);

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
  char xid_hid[60];
  sprintf(xid_hid, "HID:%040d", target_adnum);
  std::string xionnode = "XION:";
  xionnode += scion_path;
  Graph g = Node() * Node::XIONNode(xionnode) * Node(xid_hid) * Node(SID);
  g.fill_sockaddr(&ddag);
  fprintf(stderr, "%s\n", g.dag_string().c_str());

  char *payload = argv[3];

  int n;
  if ((n = Xsendto(sock, payload, strlen(payload), 0, (sockaddr *)&ddag, sizeof(sockaddr_x))) < 0) {
    fprintf(stderr, "Xsendto error\n");
    return -1;
  }
}
