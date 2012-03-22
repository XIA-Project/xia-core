// -*- related-file-name: "../include/click/xiaextheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#include <click/xiacontentheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

ContentHeaderEncap::ContentHeaderEncap(uint16_t offset, uint32_t chunk_offset, 
        uint16_t length, uint32_t chunk_length, char opcode, 
        uint32_t contextID, uint32_t ttl, uint32_t cacheSize, uint32_t cachePolicy) {
    this->map()[ContentHeader::OFFSET]= String((const char*)&offset, sizeof(offset));
    this->map()[ContentHeader::CHUNK_OFFSET]= String((const char*)&chunk_offset, sizeof(chunk_offset));
    this->map()[ContentHeader::LENGTH]= String((const char*)&length, sizeof(length));
    this->map()[ContentHeader::CHUNK_LENGTH]= String((const char*)&chunk_length, sizeof(chunk_length));
    this->map()[ContentHeader::OPCODE]= String((const char*)&opcode, sizeof(uint8_t));
    
    this->map()[ContentHeader::CONTEXT_ID]= String((const char*)&contextID, 
            sizeof(contextID));
    this->map()[ContentHeader::TTL]= String((const char*)&ttl, sizeof(ttl));
    this->map()[ContentHeader::CACHE_SIZE]= String((const char*)&cacheSize, 
            sizeof(cacheSize));
    this->map()[ContentHeader::CACHE_POLICY]= String((const char*)&cachePolicy, 
            sizeof(cachePolicy));
    this->update();
}

ContentHeaderEncap::ContentHeaderEncap(uint8_t opcode, uint32_t chunk_offset, uint16_t length)
{
    this->map()[ContentHeader::CHUNK_OFFSET]= String((const char*)&chunk_offset, sizeof(chunk_offset));
    this->map()[ContentHeader::LENGTH]= String((const char*)&length, sizeof(length));
    this->map()[ContentHeader::OPCODE]= String((const char*)&opcode, sizeof(uint8_t));
    this->update();
}

CLICK_ENDDECLS
