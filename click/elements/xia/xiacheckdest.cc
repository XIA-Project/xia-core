/*
 * xiacheckdest.{cc,hh} -- checks if the packet has arrived at the destination
 */

#include <click/config.h>
#include "xiacheckdest.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIACheckDest::XIACheckDest()
{
}

XIACheckDest::~XIACheckDest()
{
}

void
XIACheckDest::push(int, Packet *p)
{
    const struct click_xia* hdr = p->xia_header();

	// commented out for microbenchmarks
    /*
    if (!hdr)
    {
        p->kill();
        return;
    }

    if (hdr->last >= (int)hdr->dnode)
    {
        // invalid packet
        click_chatter("packet killed due to invalid last pointer\n");
        p->kill();
        return;
    }
    */

	// The definition of the destination is the node without any outgoing edge,
	// but we can also determine if the last visited node is pointing to the last node in the destination DAG
	// because it is a convention to place the destination node at the end of the node list.
    if (hdr->last == (int)hdr->dnode - 1)
        output(0).push(p);
    else
        output(1).push(p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACheckDest)
ELEMENT_MT_SAFE(XIACheckDest)
