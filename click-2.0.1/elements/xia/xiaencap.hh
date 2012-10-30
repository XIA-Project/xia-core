#ifndef CLICK_XIAENCAP_HH
#define CLICK_XIAENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiacontentheader.hh>
CLICK_DECLS

/*
=c

XIAEncap(SRC, DST)

=s xia

encapsulates packets in static XIA header

=d

Encapsulates the input packet with an XIA header with 
source address SRC and destination address DST.

=e

Wraps packets in an XIA header 
with source 0001:FFFFF...(20 byte) and destination FF:

  XIAEncap(00, FF)

=h src read/write

Returns or sets the SRC parameter.

=h dst read/write

Returns or sets the DST parameter.

=a UDPIPEncap, StripIPHeader */

class XIAEncap : public Element { public:

    XIAEncap();
    ~XIAEncap();
  
    const char *class_name() const		{ return "XIAEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
  
    Packet *simple_action(Packet *);
  
  private:
    class XIAHeaderEncap* _xiah;
    class ContentHeaderEncap* _contenth;
    bool _is_dynamic;
};

CLICK_ENDDECLS
#endif
