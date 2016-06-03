// -*- related-file-name: "../include/click/xiaextheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#include <iostream>
#endif


CLICK_DECLS

/** @file xiaextheader.hh
 */

XIAGenericExtHeader::XIAGenericExtHeader(const XIAGenericExtHeader& r)
    : _hdr(r._hdr)
{
}

XIAGenericExtHeader::XIAGenericExtHeader(const struct click_xia_ext* hdr)
    : _hdr(hdr)
{
}

XIAGenericExtHeader::XIAGenericExtHeader(const Packet* p)
    : _hdr(reinterpret_cast<const struct click_xia_ext*>(XIAHeader(p).next_header()))
{
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(const WritableXIAGenericExtHeader& r)
    : XIAGenericExtHeader(r._hdr)
{
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(struct click_xia_ext* hdr)
    : XIAGenericExtHeader(hdr)
{
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(WritablePacket* p)
    : XIAGenericExtHeader(p)
{
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap()
{
    const size_t size = sizeof(struct click_xia_ext);
    _hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);
    memset(_hdr, 0, size);
    _hdr->nxt = CLICK_XIA_NXT_NO;
    _hdr->hlen = size;
	_hdr->type = 0;
    assert(hlen() == size);
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap(const XIAGenericExtHeaderEncap& r)
{
    const size_t size = r.hlen();
    _hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);
    memcpy(_hdr, r._hdr, size);
    assert(hlen() == size);
}

XIAGenericExtHeaderEncap::~XIAGenericExtHeaderEncap()
{
    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = NULL;
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap(const XIAGenericExtHeader& r)
{
    const size_t size = r.hlen();
    _hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);
    memcpy(_hdr, r.hdr(), size);
    assert(hlen() == size);
}

const struct click_xia_ext*
XIAGenericExtHeaderEncap::hdr() const
{
    return _hdr;
}

struct click_xia_ext*
XIAGenericExtHeaderEncap::hdr()
{
    return _hdr;
}

const uint8_t&
XIAGenericExtHeaderEncap::hlen() const
{
    return _hdr->hlen;
}

void
XIAGenericExtHeaderEncap::set_nxt(uint8_t nxt)
{
    _hdr->nxt = nxt;
}

void
XIAGenericExtHeaderEncap::set_type(uint8_t type)
{
    _hdr->type = type;
}

WritablePacket*
XIAGenericExtHeaderEncap::encap(Packet* p_in) const
{
    size_t len = hlen();    // this call also set _hdr->hlen
    WritablePacket* p = p_in->push(len);
    if (!p)
        return NULL;

    memcpy(p->data(), _hdr, hlen());  // copy the header
    return p;
}

CLICK_ENDDECLS
