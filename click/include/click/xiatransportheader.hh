// -*- c-basic-offset: 4; related-file-name: "../../lib/xiatransportheader.cc" -*-
#ifndef CLICK_TRANSPORTEXTHEADER_HH
#define CLICK_TRANSPORTEXTHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/xiaheader.hh>
#include <click/xiaextheader.hh>
#include <click/xid.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

class TransportHeaderEncap;

class TransportHeader : public XIAGenericExtHeader { public:
    TransportHeader(const struct click_xia_ext* hdr) :XIAGenericExtHeader(hdr) {
		_tcphdr = (struct click_tcp*)(hdr + sizeof(struct click_xia_ext));
	};

	TransportHeader(const Packet* p):XIAGenericExtHeader(p) {
		_tcphdr = (struct click_tcp*)(_hdr + sizeof(struct click_xia_ext));
	};

    uint8_t type() { return _type; };
    const void* header() {return _tcphdr; };
    u_char* tcpopt() { return (u_char*)_tcphdr + sizeof(struct click_tcp); };
	uint32_t seq_num() { return _tcphdr->th_seq; };
	uint32_t ack_num() {  return _tcphdr->th_ack; };
	uint32_t recv_window() { return _tcphdr->th_win; };

	// FIXME: not needed?
	// uint16_t length() { return _length; };
	// uint8_t pkt_info() { return _pktinfo; };
	// uint8_t opcode() { if (!exists(OPCODE)) return 0 ; return *(const uint8_t*)_map[OPCODE].data();};

//    enum { TYPE, HEADER, TCPOPT, PKT_INFO, SRC_XID, DST_XID, SEQ_NUM, ACK_NUM, LENGTH, RECV_WINDOW};
	enum { SYN=1, SYNACK, DATA, ACK, FIN, FINACK, MIGRATE, MIGRATEACK, RST};

//    enum { XSOCK_STREAM=1, XSOCK_DGRAM, XSOCK_RAW, XSOCK_CHUNK};

    static const char *TypeStr(char type);

private:
	struct click_tcp *_tcphdr;
	uint8_t _type;
	// uint16_t _length;
	// uint8_t _pktinfo;
};

class TransportHeaderEncap : public XIAGenericExtHeaderEncap { public:

    /* data length contained in the packet*/
    //TransportHeaderEncap(uint16_t offset, uint32_t chunk_offset, uint16_t length, uint32_t chunk_length, char opcode= TransportHeader::OP_RESPONSE);

    //TransportHeaderEncap(char type, char pkt_info, XID src_xid, XID dst_xid, uint32_t seq_num, uint32_t ack_num, uint16_t length);
//    TransportHeaderEncap(char type, char pkt_info, uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window);

    //static TransportHeaderEncap* MakeRequestHeader() { return new TransportHeaderEncap(TransportHeader::OP_REQUEST,0,0); };
    //static TransportHeaderEncap* MakeRPTRequestHeader() { return new TransportHeaderEncap(TransportHeader::OP_REDUNDANT_REQUEST,0,0); };
    TransportHeaderEncap(char type);

    static TransportHeaderEncap* MakeTCPHeader(click_tcp *tcph) {
    	TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_STREAM);

		assert(tcph != NULL);
		memcpy(&hdr->_tcphdr, tcph, sizeof(struct click_tcp));
    	hdr->update();
     	return hdr;
    }

    static TransportHeaderEncap* MakeTCPHeader(click_tcp *tcph, u_char *opt, unsigned optlen) {
		assert(tcph != NULL);

    	TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_STREAM);

		memcpy(&hdr->_tcphdr, tcph, sizeof(struct click_tcp));
		hdr->_options = (u_char*)malloc(optlen);
		memcpy(hdr->_options, opt, optlen);
		hdr->_optlen = optlen;

    	// hdr->map()[TransportHeader::HEADER] = String((const char*)tcph, sizeof(struct click_tcp));
    	// hdr->map()[TransportHeader::TCPOPT] = String((const char*)opt, optlen);
    	hdr->update();
     	return hdr;
    }

	static TransportHeaderEncap* MakeDGRAMHeader(uint16_t /* length */) {
		TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_DGRAM);
		return hdr;
	}

	~TransportHeaderEncap() {
		delete _tcphdr;
		delete _options;
		_tcphdr = NULL;
		_options = NULL;
	}

	void update();

private:
	uint8_t _type;
	struct click_tcp *_tcphdr;
	u_char *_options;
	unsigned _optlen;

/*
    static TransportHeaderEncap* MakeSYNHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::SYN, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeSYNACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::SYNACK, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeDATAHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::DATA, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::ACK, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeFINHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::FIN, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeFINACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::FINACK, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeRSTHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::RST, seq_num, ack_num, length, recv_window); };
    static TransportHeaderEncap* MakeMIGRATEHeader(uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::MIGRATE, seq_num, ack_num, length, recv_window); };

    static TransportHeaderEncap* MakeMIGRATEACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::MIGRATEACK, seq_num, ack_num, length, recv_window); };
*/
};


CLICK_ENDDECLS
#endif
