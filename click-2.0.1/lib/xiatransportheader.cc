// -*- related-file-name: "../include/click/xiatransportheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#include <click/xiatransportheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

TransportHeaderEncap::TransportHeaderEncap(char type, char pkt_info, uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window) {
    this->map()[TransportHeader::TYPE]= String((const char*)&type, sizeof(type));
    this->map()[TransportHeader::PKT_INFO]= String((const char*)&pkt_info, sizeof(pkt_info));
    //this->map()[TransportHeader::SRC_XID]= String((const char*)&src_xid, sizeof(src_xid));
    //this->map()[TransportHeader::DST_XID]= String((const char*)&dst_xid, sizeof(dst_xid));
    this->map()[TransportHeader::SEQ_NUM]= String((const char*)&seq_num, sizeof(seq_num));
    this->map()[TransportHeader::ACK_NUM]= String((const char*)&ack_num, sizeof(ack_num));        
    this->map()[TransportHeader::LENGTH]= String((const char*)&length, sizeof(length));
	this->map()[TransportHeader::RECV_WINDOW]= String((const char*)&recv_window, sizeof(recv_window));
    this->update();
}

const char *TransportHeader::TypeStr(char type)
{
    const char *t;

    switch (type) {
        case SYN:        t = "SYN"; break;
        case SYNACK:     t = "SYNACK"; break;
        case DATA:       t = "DATA"; break;
        case ACK:        t = "ACK"; break;
        case FIN:        t = "FIN"; break;
        case FINACK:     t = "FINACK"; break;
        case MIGRATE:    t = "MIGRATE"; break;
        case MIGRATEACK: t = "MIGRATEACK"; break;
        case RST:        t = "RST"; break;
    }
    return t;
}
/*
TransportHeaderEncap::TransportHeaderEncap(uint8_t opcode, uint32_t chunk_offset, uint16_t length)
{
    this->map()[TransportHeader::CHUNK_OFFSET]= String((const char*)&chunk_offset, sizeof(chunk_offset));
    this->map()[TransportHeader::LENGTH]= String((const char*)&length, sizeof(length));
    this->map()[TransportHeader::OPCODE]= String((const char*)&opcode, sizeof(uint8_t));
    this->update();
}
*/

CLICK_ENDDECLS
