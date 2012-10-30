#ifndef CLICK_XIAPINGRESPONDER_HH
#define CLICK_XIAPINGRESPONDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
CLICK_DECLS

/*
=c

XIAPingResponder(SRC)

=s xia

responds to XIA ping requests at SRC.

=d

Sends XIA pong packets in response to incoming XIA ping packets,
while updating the address when a notification message is received.

=e

XIAPingResponder(RE AD0 RHID0 HID0)

*/

class XIAPingResponder : public Element { public:

    XIAPingResponder();
    ~XIAPingResponder();
  
    const char *class_name() const		{ return "XIAPingResponder"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return PUSH; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
  
    void push(int, Packet *);
  
  private:
    XIAPath _src_path;
    uint32_t _count;
  
    bool _connected;
    XIAPath _last_client;
};

CLICK_ENDDECLS
#endif
