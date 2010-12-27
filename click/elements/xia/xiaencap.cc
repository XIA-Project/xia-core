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
    : _xiah(NULL)
{
    _xiah = new XIAHeader(0);
}

XIAEncap::~XIAEncap()
{
    delete _xiah;
}

int
XIAEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String dst_str;
    String src_str;
    int nxt = -1;
    int last = -1;
    uint8_t hlim = 250;

    if (cp_va_kparse(conf, this, errh,
                   "NXT", cpkP+cpkM, cpNamedInteger, NameInfo::T_IP_PROTO, &nxt,
                   "SRC", cpkP+cpkM, cpArgument, &src_str,
                   "DST", cpkP+cpkM, cpArgument, &dst_str,
                   "LAST", 0, cpInteger, &last,
                   "HLIM", 0, cpByte, &hlim,
                   cpEnd) < 0)
        return -1;

    if (nxt < 0 || nxt > 255)
        return errh->error("bad next protocol");

    XIAPath dst_path;
    XIAPath src_path;

    String dst_type = cp_shift_spacevec(dst_str);
    if (dst_type == "DAG")
        dst_path.parse_dag(dst_str, this);
    else if (dst_type == "RE")
        dst_path.parse_re(dst_str, this);
    else
        return errh->error("unrecognized dst type: %s", dst_type.c_str());

    String src_type = cp_shift_spacevec(src_str);
    if (src_type == "DAG")
        src_path.parse_dag(src_str, this);
    else if (src_type == "RE")
        src_path.parse_re(src_str, this);
    else
        return errh->error("unrecognized src type: %s", src_type.c_str());

    delete _xiah;   // this is safe even when _xiah == NULL

    _xiah = new XIAHeader();
    if (!_xiah)
        return errh->error("failed to allocate");

    _xiah->hdr().nxt = nxt;
    _xiah->hdr().plen = 0;
    _xiah->hdr().last = last;
    _xiah->hdr().hlim = hlim;

    _xiah->set_dst_path(dst_path);
    _xiah->set_src_path(dst_path);

    return 0;
}


int
XIAEncap::initialize(ErrorHandler *)
{
    // do some alignment/packing test
    assert(sizeof(struct click_xia) == 8);
    assert(sizeof(struct click_xia_xid) == 21);
    assert(sizeof(struct click_xia_xid_edge) == 1);
    assert(sizeof(struct click_xia_xid_node) == 24);
    struct click_xia hdr;
    assert(reinterpret_cast<unsigned long>(&(hdr.node[0])) - reinterpret_cast<unsigned long>(&hdr) == 8);
    assert(reinterpret_cast<unsigned long>(&(hdr.node[1])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 24);
    (void)hdr;
    return 0;
}


Packet *
XIAEncap::simple_action(Packet *p_in)
{
    int header_len = _xiah->size();
    int payload_len = p_in->length();
    WritablePacket *p = p_in->push(header_len); // make room for XIA header
    if (!p) return 0;

    click_xia *xiah = reinterpret_cast<click_xia *>(p->data());
    memcpy(xiah, &_xiah->hdr(), header_len); // copy the header
    xiah->plen = htons(payload_len);    // original payload length excluding new header length

    p->set_xia_header(xiah, header_len); // set the network header position (nh)
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
