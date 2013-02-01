#ifndef CLICK_XIAFILTERPACKETS_HH
#define CLICK_XIAFILTERPACKETS_HH
#include <click/element.hh>
#include <clicknet/udp.h>
CLICK_DECLS

/*
=c

XIAFilterPackets(COLOR [, ANNO])

=s



=d

=h 



=a */

class XIAFilterPackets : public Element { public:

    XIAFilterPackets();
    ~XIAFilterPackets();

    const char *class_name() const		{ return "XIAFilterPackets"; }
    const char *port_count() const		{ return "1/2"; }
  const char *processing() const  { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const {return false;}

  void push(int port, Packet *);

  private:

  uint16_t _port;
};

CLICK_ENDDECLS
#endif
