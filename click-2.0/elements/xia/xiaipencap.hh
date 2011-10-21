#ifndef CLICK_XIAIPENCAP_HH
#define CLICK_XIAIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
#include <clicknet/xia.h>
#include <click/xid.hh>
CLICK_DECLS

/*
 * =c
 * XIAIPEncap()
 * =s ip
 * Ecapsulates an XIA IP XID Packet into an IP Packet
 * =d
 *
 *
 * =n
 *
 *
 * =a ICMPError */

#define IP_XID_PROTO 250

class XIAIPEncap : public Element {

public:
  XIAIPEncap();
  ~XIAIPEncap();

  const char *class_name() const		{ return "XIAIPEncap"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);

protected:
  click_ip _iph;
  atomic_uint32_t _id;

  inline void update_cksum(click_ip *, int) const;
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
