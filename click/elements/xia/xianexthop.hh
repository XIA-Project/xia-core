#ifndef CLICK_XIANEXTHOP_HH
#define CLICK_XIANEXTHOP_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
CLICK_DECLS

/*
=c
XIANextHop()

=s ip
adjusts the "last" pointer when the packet has arrived at its next hop

=d
Adjusts the "last" pointer when the packet has arrived at its next hop.
The packet must be handled by XIAXIDRouteTable first for painting.

=e

XIANextHop()

=a StaticXIDLookup
*/

class XIANextHop : public Element { public:

    XIANextHop();
    ~XIANextHop();

    const char *class_name() const		{ return "XIANextHop"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PUSH; }

    void push(int port, Packet *);
};

CLICK_ENDDECLS
#endif
