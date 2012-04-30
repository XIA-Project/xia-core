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
    populate_map();
}

XIAGenericExtHeader::XIAGenericExtHeader(const struct click_xia_ext* hdr)
    : _hdr(hdr)
{
    populate_map();
}

XIAGenericExtHeader::XIAGenericExtHeader(const Packet* p)
    : _hdr(reinterpret_cast<const struct click_xia_ext*>(XIAHeader(p).next_header()))
{
    populate_map();
}

void
XIAGenericExtHeader::populate_map()
{
    _map.clear();

    const uint8_t* d = _hdr->data;
    const uint8_t* end = reinterpret_cast<const uint8_t*>(_hdr) + _hdr->hlen;
    while (d < end) {
        uint8_t kv_len = *d++;

        if (!kv_len) {
            // hit padding
            break;
        }

        if (d + kv_len > end) {
            click_chatter("invalid kv_len or hlen");
            break;
        }

        uint8_t key = *d;
        String value(reinterpret_cast<const char*>(d + 1), kv_len - 1);
        _map[key] = value;
        d += kv_len;
    }
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
    assert(hlen() == size);
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap(const XIAGenericExtHeaderEncap& r)
{
    const size_t size = r.hlen();
    _hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]); 
    memcpy(_hdr, r._hdr, size);
    _map = r._map;
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
    _map = r.map();
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
XIAGenericExtHeaderEncap::update()
{
    size_t size = sizeof(struct click_xia_ext);
    HashTable<uint8_t, String>::const_iterator it = _map.begin();
    size_t count = 0;
    size_t padding = 0;
    for (; it != _map.end(); ++it) {
        if ((*it).second.length() >= 255 - 1) {   // skip too long value
            click_chatter("too long value for key %d", (*it).first);
            continue;
        }
        size_t new_size = size + 2 + (*it).second.length();
        if (new_size >= 255) {
            click_chatter("too large key-value map");
            break;
        }
        size = new_size;
        count++;
    }
    if ((size & 3) != 0) {
        padding = 4 - (size & 3);
        size += padding;
    }

    click_xia_ext* new_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);

    // preserve the current header content except key-value map
    memcpy(new_hdr, _hdr, sizeof(struct click_xia_ext));

    // update the new header
    new_hdr->hlen = size;

    uint8_t* d = new_hdr->data;
    for (it = _map.begin(); it != _map.end() && count > 0; ++it, count--) {
        if ((*it).second.length() >= 255 - 1)   // skip too long value
            continue;
        // key-value length
        *d++ = 1 + (*it).second.length();
        // key
        *d++ = (*it).first;
        // value
        memcpy(d, (*it).second.data(), (*it).second.length());
        d += (*it).second.length();
    }
    // padding
    memset(d, 0, padding);

    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = new_hdr;
}

WritablePacket*
XIAGenericExtHeaderEncap::encap(Packet* p_in) const
{
    size_t len = hlen();    // this call also set _hdr->hlen
    WritablePacket* p = p_in->push(len);
    if (!p)
        return NULL;

    /*
    for (HashTable<uint8_t, String>::const_iterator it = _map.begin(); it != _map.end(); ++it)
        click_chatter("%d %d %02hhx %02hhx\n", (*it).first, (*it).second.length(), (*it).second.c_str()[0], (*it).second.c_str()[1]); 
    */

    memcpy(p->data(), _hdr, hlen());  // copy the header

    return p;
}

CLICK_ENDDECLS
