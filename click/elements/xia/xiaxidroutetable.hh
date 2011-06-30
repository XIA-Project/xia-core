#ifndef CLICK_XIAXIDROUTETABLE_HH
#define CLICK_XIAXIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
CLICK_DECLS

/*
=c
XIAXIDRouteTable(XID1 OUT1, XID2 OUT2, ..., - OUTn)

=s ip
simple XID routing table

=d
Routes XID according to a routing table.

=e

XIAXIDRouteTable(AD0 0, HID2 1, - 2)
It outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.
If the packet has already arrived at the destination node, the packet will be destroyed,
so use the XIACheckDest element before using this element.

=a StaticIPLookup, IPRouteTable
*/

class XIAXIDRouteTable : public Element { public:

    XIAXIDRouteTable();
    ~XIAXIDRouteTable();

    const char *class_name() const		{ return "XIAXIDRouteTable"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int port, Packet *);

protected:
    int lookup_route(Packet *);

    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
#if CLICK_USERLEVEL
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
#endif
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);

private:
    HashTable<XID, int> _rt;
    int _rem;
};

CLICK_ENDDECLS
#endif
