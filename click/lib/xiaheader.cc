// -*- related-file-name: "../include/click/xiaheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

/** @file xiaheader.hh
 * @brief The Packet class models packets in Click.
 */

XIAHeader::XIAHeader(size_t dsnode)
{
    const size_t size = XIAHeader::size(dsnode);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memset(_hdr, 0, size);
}

XIAHeader::XIAHeader(const struct click_xia& hdr)
{
    const size_t size = XIAHeader::size(hdr.dsnode);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memcpy(_hdr, &hdr, size);
}

XIAHeader::~XIAHeader()
{
    delete [] _hdr;
    _hdr = NULL;
}

CLICK_ENDDECLS
