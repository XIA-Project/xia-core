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

    uint8_t opcode() { if (!exists(OPCODE)) return 0 ; return *(const uint8_t*)_map[OPCODE].data();};  

    bool exists(uint8_t key) { return (_map.find(key)!=_map.end()); }

    
    uint16_t offset() { if (!exists(OFFSET)) return 0; return *(const uint16_t*)_map[OFFSET].data();};  
    uint32_t chunk_offset() { if (!exists(CHUNK_OFFSET)) return 0; return *(const uint32_t*)_map[CHUNK_OFFSET].data();};  
    uint16_t length() { if (!exists(LENGTH)) return 0; return *(const uint16_t*)_map[LENGTH].data();};  
    uint32_t chunk_length() { if (!exists(CHUNK_LENGTH)) return 0; return *(const uint32_t*)_map[CHUNK_LENGTH].data();};  
    
    uint32_t contextID() { 
        if (!exists(CONTEXT_ID)) 
            return 0; 
        return *(const uint32_t*)_map[CONTEXT_ID].data();
    };  
    uint32_t ttl() { 
        if (!exists(TTL)) 
            return 0; 
        return *(const uint32_t*)_map[TTL].data();
    };  
    uint32_t cacheSize() { 
        if (!exists(CACHE_SIZE)) 
            return 0; 
        return *(const uint32_t*)_map[CACHE_SIZE].data();
    };  
    uint32_t cachePolicy() { 
        if (!exists(CACHE_POLICY)) 
            return 0; 
        return *(const uint32_t*)_map[CACHE_POLICY].data();
    };  
    
    enum { OPCODE, OFFSET, CHUNK_OFFSET, LENGTH, CHUNK_LENGTH, CONTEXT_ID, TTL, CACHE_SIZE, CACHE_POLICY}; 
    enum { OP_REQUEST=1, OP_RESPONSE, OP_LOCAL_PUTCID, OP_REDUNDANT_REQUEST, OP_LOCAL_REMOVECID};
};

class ContentHeaderEncap : public XIAGenericExtHeaderEncap { public:

    /* data length contained in the packet*/
    ContentHeaderEncap(uint16_t offset, uint32_t chunk_offset, uint16_t length, 
            uint32_t chunk_length, char opcode= ContentHeader::OP_RESPONSE, 
            uint32_t contextID=0, uint32_t ttl = 86400, uint32_t cacheSize=1024, uint32_t cachePolicy=0);

    ContentHeaderEncap(uint8_t opcode, uint32_t chunk_offset=0, uint16_t length=0);

    static ContentHeaderEncap* MakeRequestHeader() { return new ContentHeaderEncap(ContentHeader::OP_REQUEST,0,0); };
    static ContentHeaderEncap* MakeRPTRequestHeader() { return new ContentHeaderEncap(ContentHeader::OP_REDUNDANT_REQUEST,0,0); };
    static ContentHeaderEncap* MakeRequestHeader( uint32_t chunk_offset, uint16_t length ) 
                        { return new ContentHeaderEncap(ContentHeader::OP_REQUEST,chunk_offset, length); };

};


CLICK_ENDDECLS
#endif
