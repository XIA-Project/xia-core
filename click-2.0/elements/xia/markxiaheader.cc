/*
 * markxiaheader.{cc,hh} -- element sets XIA Header annotation
 */

#include <click/config.h>
#include "markxiaheader.hh"
#include <click/confparse.hh>
#include <clicknet/xia.h>
#include <click/xiaheader.hh>
CLICK_DECLS

MarkXIAHeader::MarkXIAHeader()
{
}

MarkXIAHeader::~MarkXIAHeader()
{
}

int
MarkXIAHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _offset = 0;
    return cp_va_kparse(conf, this, errh,
  		      "OFFSET", cpkP, cpUnsigned, &_offset,
  		      cpEnd);
}

Packet *
MarkXIAHeader::simple_action(Packet *p)
{
    const click_xia *xiah = reinterpret_cast<const click_xia *>(p->data() + _offset);

    p->set_xia_header(xiah, XIAHeader::hdr_size(xiah->dnode + xiah->snode));

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MarkXIAHeader)
ELEMENT_MT_SAFE(MarkXIAHeader)
