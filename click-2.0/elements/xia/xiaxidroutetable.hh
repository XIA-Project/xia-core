#ifndef CLICK_XIAXIDROUTETABLE_HH
#define CLICK_XIAXIDROUTETABLE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiapath.hh>
#include "xcmp.hh"
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

typedef struct {
	int	port;
	unsigned flags;
	XID *nexthop;
} XIARouteData;

class XIAXIDRouteTable : public Element { public:

    XIAXIDRouteTable();
    ~XIAXIDRouteTable();

    const char *class_name() const		{ return "XIAXIDRouteTable"; }
    const char *port_count() const		{ return "-/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    void push(int in_ether_port, Packet *);

protected:
    int lookup_route(int in_ether_port, Packet *);
    int process_xcmp_redirect(Packet *);

    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int set_handler4(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
    static int remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
    static int generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh);

    static String list_routes_handler(Element *e, void *thunk);

private:
	HashTable<XID, XIARouteData*> _rts;
	XIARouteData _rtdata;
    uint32_t _drops;

    XIAPath _local_addr;
    XID _local_hid;
    XID _bcast_xid;
    int _redirect_port, _bcast_port, _my_port;
};

CLICK_ENDDECLS
#endif
