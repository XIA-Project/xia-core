#ifndef CLICK_STATICXIDLOOKUP_HH
#define CLICK_STATICXIDLOOKUP_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
CLICK_DECLS

/*
=c
StaticXIDLookup(XID1 OUT1, XID2 OUT2, ..., - OUTn, dest OUTm)

=s ip
simple static XID routing table

=d
Routes XID according to a static routing table.

=e

StaticXIDLookup(AD0 0, HID2 1, - 2, dest 3)
It outputs arrived packets to port 3, and
outputs AD0 packets to port 0, HID2 packets to port 1, and other packets to port 2.

=a StaticIPLookup
*/

class StaticXIDLookup : public Element { public:

    StaticXIDLookup();
    ~StaticXIDLookup();

    const char *class_name() const		{ return "StaticXIDLookup"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);

    void push(int port, Packet *);

protected:
    int lookup(Packet *);

private:
    HashTable<XID, int> _rt;
    int _rem;
    int _dest;
};

CLICK_ENDDECLS
#endif
