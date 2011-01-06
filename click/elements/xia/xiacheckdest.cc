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

    if (hdr->last == (int)hdr->dnode - 1)
        checked_output_push(0, p);
    else
        checked_output_push(1, p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACheckDest)
ELEMENT_MT_SAFE(XIACheckDest)
