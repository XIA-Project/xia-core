#ifndef CLICK_XIACHECKDEST_HH
#define CLICK_XIACHECKDEST_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
CLICK_DECLS

/*
=c
XIACheckDest()

=s xip
checks if the packet has arrived at the destination

=d
Checks if the packet has arrived at the destination.

=e

XIACheckDest()
It outputs the input packet to port 0 if it has arrived at the destination node,
or port 1 otherwise.

=a 
*/

class XIACheckDest : public Element { public:

    XIACheckDest();
    ~XIACheckDest();

    const char *class_name() const		{ return "XIACheckDest"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return PUSH; }

    void push(int port, Packet *);

  protected:
    int lookup(Packet *);
};

CLICK_ENDDECLS
#endif
