#ifndef CLICK_DYNAMICIPENCAP_HH
#define CLICK_DYNAMICIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
#include "ipencap.hh"
CLICK_DECLS

class DynamicIPEncap : public IPEncap { public:
  DynamicIPEncap();
  ~DynamicIPEncap();

  const char *class_name() const		{ return "DynamicIPEncap"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);
  uint32_t _max_count;
  uint32_t _count;
};

CLICK_ENDDECLS
#endif
