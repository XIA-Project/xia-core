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
    : _xiah(NULL), _contenth(NULL)
{
    _xiah = new XIAHeaderEncap();
}

XIAEncap::~XIAEncap()
{
    delete _xiah;
    delete _contenth;
}

int
XIAEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String dst_str;
    String src_str;
    int nxt = -1;
    int last = -1;
    uint8_t hlim = 250;
    int packet_offset =-1, chunk_offset =-1, content_length =-1, chunk_length =-1;

    if (cp_va_kparse(conf, this, errh,
                   "NXT", cpkP+cpkM, cpNamedInteger, NameInfo::T_IP_PROTO, &nxt,
                   "SRC", cpkP+cpkM, cpArgument, &src_str,
                   "DST", cpkP+cpkM, cpArgument, &dst_str,
                   "LAST", 0, cpInteger, &last,
                   "HLIM", 0, cpByte, &hlim,
                   "EXT_C_PACKET_OFFSET", 0, cpInteger, &packet_offset,
                   "EXT_C_CHUNK_OFFSET", 0, cpInteger, &chunk_offset,
                   "EXT_C_CONTENT_LENGTH", 0, cpInteger, &content_length,
                   "EXT_C_CHUNK_LENGTH", 0, cpInteger, &chunk_length,
                   cpEnd) < 0)
        return -1;

    if (nxt < 0 || nxt > 255)
        return errh->error("bad next protocol");

    XIAPath dst_path;
    XIAPath src_path;

    String dst_type = cp_shift_spacevec(dst_str);
    if (dst_type == "DAG")
    {
        if (!dst_path.parse_dag(dst_str, this))
            return errh->error("unable to parse DAG: %s", dst_str.c_str());
    }
    else if (dst_type == "RE")
    {
        if (!dst_path.parse_re(dst_str, this))
            return errh->error("unable to parse DAG: %s", dst_str.c_str());
    }
    else
        return errh->error("unrecognized dst type: %s", dst_type.c_str());

    String src_type = cp_shift_spacevec(src_str);
    if (src_type == "DAG")
    {
        if (!src_path.parse_dag(src_str, this))
            return errh->error("unable to parse DAG: %s", src_str.c_str());
    }
    else if (src_type == "RE")
    {
        if (!src_path.parse_re(src_str, this))
            return errh->error("unable to parse DAG: %s", src_str.c_str());
    }
    else
        return errh->error("unrecognized src type: %s", src_type.c_str());

    _xiah->set_nxt(nxt);
    _xiah->set_last(last);
    _xiah->set_hlim(hlim);
    _xiah->set_dst_path(dst_path);
    _xiah->set_src_path(src_path);

    if (chunk_length!=-1) {
        _contenth  = new ContentHeaderEncap(packet_offset, chunk_offset, content_length, chunk_length);
        _contenth->set_nxt(nxt);
        _contenth->update();
        _xiah->set_nxt(CLICK_XIA_NXT_CID);
        click_chatter("EXT %d %d %d %d\n", packet_offset, chunk_offset, content_length, chunk_length );
    }

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
    WritablePacket *p = NULL;
    size_t length = p_in->length();
    if (_contenth) {
        p = _contenth->encap(p_in);
        if (p) {
            _xiah->set_plen(length); // set payload length ignoring the content ext header
            p = _xiah->encap(p, false);
        }
    }
    else
        p = _xiah->encap(p_in, true);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
