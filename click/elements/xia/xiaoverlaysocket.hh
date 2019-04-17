#ifndef XIA_OVERLAY_SOCKET_HH
#define XIA_OVERLAY_SOCKET_HH
#include <click/element.hh>
#include "../userlevel/socket.hh"
CLICK_DECLS

/*
 * Allow XIA to run as an overlay on UDP/IP
 */
class XIAOverlaySocket : public Socket { public:

  const char *class_name() const	{ return "XIAOverlaySocket"; }

  bool run_task(Task *);
  void selected(int fd, int mask);
  void push(int port, Packet*);

  int write_packet(Packet*);
};

CLICK_ENDDECLS
#endif // XIA_OVERLAY_SOCKET_HH
