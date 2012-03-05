/*
 * xianexthop.{cc,hh} -- advance "last" pointer and update edge information
 */

#include <click/config.h>
#include "xianexthop.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIANextHop::XIANextHop()
{
}

XIANextHop::~XIANextHop()
{
}

void
XIANextHop::push(int, Packet *p_in)
{
    WritablePacket* p = p_in->uniqueify();

    struct click_xia* hdr = p->xia_header();

    // skip error checking assuming it has beein already done by StaticXIDLookup

    int last = hdr->last;
    if (last < 0)
        last += hdr->dnode;

    struct click_xia_xid_edge* edge = hdr->node[last].edge;
    struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p_in)];

    hdr->last = current_edge.idx;
    current_edge.visited = 1;

    output(0).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIANextHop)
ELEMENT_MT_SAFE(XIANextHop)
