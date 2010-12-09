/*
 * xiancap.{cc,hh} -- element encapsulates packet in XIA header
 * Dongsu Han 
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "xiaencap.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAEncap::XIAEncap()
{
}

XIAEncap::~XIAEncap()
{
}

int
XIAEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  struct click_xid_v1 src, dst;

  XIAHeader headerbuilder;

  if (cp_va_kparse(conf, this, errh,
		   "SRC", cpkP+cpkM, cpXID, &src,
		   "DST", cpkP+cpkM, cpXID, &dst,
		   cpEnd) < 0)
    return -1;

  int srcid = headerbuilder.addXID(src);
  headerbuilder.setSourceStack(srcid);
  int dstid = headerbuilder.addXID(dst);


  headerbuilder.connectXID(0, srcid, dstid);
  _xiah = headerbuilder.header();

  return 0;
}

int
XIAEncap::initialize(ErrorHandler *)
{
  return 0;
}


Packet *
XIAEncap::simple_action(Packet *p_in)
{
    int header_len = _xiah->len();
    int payload_len = htons(p_in->length());
    WritablePacket *p = p_in->push(header_len); // make room for XIA header
    if (!p) return 0;


    click_xia *xiah = reinterpret_cast<click_xia *>(p->data());
    memcpy(xiah, _xiah, header_len); // copy the header
    p->set_xia_header(xiah, header_len); // set the network header position (nh)
    xiah->payload_len = payload_len;
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
