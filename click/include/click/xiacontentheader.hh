// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaextheader.cc" -*-
#ifndef CLICK_CONTENTEXTHEADER_HH
#define CLICK_CONTENTEXTHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/xiaheader.hh>
#include <click/xiaextheader.hh>

CLICK_DECLS

class ContentHeaderEncap;

class ContentHeader : public XIAGenericExtHeader { public:
    ContentHeader(const struct click_xia_ext* hdr) :XIAGenericExtHeader(hdr) {};
    ContentHeader(const Packet* p):XIAGenericExtHeader(p) {};

    uint16_t offset() { return *(const uint16_t*)_map[OFFSET].data();};  
    uint32_t chunk_offset() { return *(const uint32_t*)_map[CHUNK_OFFSET].data();};  
    uint16_t length() { return *(const uint16_t*)_map[LENGTH].data();};  
    uint32_t chunk_length() { return *(const uint32_t*)_map[CHUNK_LENGTH].data();};  
    enum { OFFSET, CHUNK_OFFSET, LENGTH, CHUNK_LENGTH}; 
};

class ContentHeaderEncap : public XIAGenericExtHeaderEncap { public:

    /* data length contained in the packet*/
    ContentHeaderEncap(uint16_t offset, uint32_t chunk_offset, uint16_t length, uint32_t chunk_length);

};


CLICK_ENDDECLS
#endif
