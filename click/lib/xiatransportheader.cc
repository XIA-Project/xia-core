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

TransportHeaderEncap::TransportHeaderEncap(char type)
{
	memset(&_tcphdr, 0, sizeof(struct click_tcp));
	_type = type;
	_options = NULL;
	_optlen = 0;

    //this->map()[TransportHeader::TYPE]= String((const char*)&type, sizeof(type));
    update();
}

#if 0
TransportHeaderEncap::TransportHeaderEncap(char type, char pkt_info, uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window)
{
	memset(&_tcphdr, 0, sizeof(tcphdr));
	_type = type;
	_pktinfo = pkt_info;
	_tcphdr.th_seq = seq_num;
	_tcphdr.th_ack = ack_num;
	_tcphdr.th_win = recv_window;
	_length = length;


//	this->map()[TransportHeader::TYPE]= String((const char*)&type, sizeof(type));
//	this->map()[TransportHeader::PKT_INFO]= String((const char*)&pkt_info, sizeof(pkt_info));
//	this->map()[TransportHeader::SEQ_NUM]= String((const char*)&seq_num, sizeof(seq_num));
//	this->map()[TransportHeader::ACK_NUM]= String((const char*)&ack_num, sizeof(ack_num));
//	this->map()[TransportHeader::LENGTH]= String((const char*)&length, sizeof(length));
//	this->map()[TransportHeader::RECV_WINDOW]= String((const char*)&recv_window, sizeof(recv_window));
    this->update();
}
#endif

void
TransportHeaderEncap::update()
{
	size_t padding = 0;
    size_t size = sizeof(struct click_xia_ext);

	size += sizeof(struct click_tcp);
	size += _optlen;

	if ((size & 3) != 0) {
        padding = 4 - (size & 3);
        size += padding;
    }

    click_xia_ext* new_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);

	struct click_tcp *t = (struct click_tcp*)(new_hdr + sizeof(struct click_xia_ext));
	u_char *o = (u_char*)(t + sizeof(struct click_tcp));

    memcpy(new_hdr, _hdr, sizeof(struct click_xia_ext));
	memcpy(t, _tcphdr, sizeof(struct click_tcp));
	memcpy(o, _options, _optlen);

	if (padding) {
		memset(o + _optlen, 0, padding);
	}

    new_hdr->hlen = size;

    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = new_hdr;
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
		default:         t = "UNKNOWN"; break;
    }
    return t;
}

CLICK_ENDDECLS
