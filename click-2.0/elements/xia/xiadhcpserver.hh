#ifndef CLICK_XIADHCPSERVER_HH
#define CLICK_XIADHCPSERVER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/timer.hh>
CLICK_DECLS

class XIADHCPServer : public Element {
 public:
  XIADHCPServer();
  ~XIADHCPServer();

  const char *class_name() const		{ return "XIADHCPServer"; }
  const char *port_count() const		{ return PORTS_0_1; }
  const char *processing() const                { return PULL; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *pull(int);

 private:
  XIAPath _src_path;
  XIAPath _broadcast_path;
  XIAPath _ad_path;
};

CLICK_ENDDECLS
#endif
