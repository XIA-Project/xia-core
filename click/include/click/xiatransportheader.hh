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
		char *h = (char*)hdr + sizeof(struct click_xia_ext);
		_tcphdr = (struct click_tcp*)h;
	};

	TransportHeader(const Packet* p):XIAGenericExtHeader(p) {
		char *h = (char*)_hdr + sizeof(struct click_xia_ext);
		_tcphdr = (struct click_tcp*)h;
	};

    const void* header() {return _tcphdr; };
    u_char* tcpopt() { return (u_char*)_tcphdr + sizeof(struct click_tcp); };
	uint32_t seq_num() { return _tcphdr->th_seq; };
	uint32_t ack_num() {  return _tcphdr->th_ack; };
	uint32_t recv_window() { return _tcphdr->th_win; };
	uint8_t flags() {return _tcphdr->th_flags; };

private:
	struct click_tcp *_tcphdr;
};

class TransportHeaderEncap : public XIAGenericExtHeaderEncap { public:

    TransportHeaderEncap(char type);

    static TransportHeaderEncap* MakeTCPHeader(click_tcp *tcph) {
    	TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_STREAM);

		assert(tcph != NULL);
		hdr->_tcphdr = (struct click_tcp*)malloc(sizeof(struct click_tcp));
		memcpy(hdr->_tcphdr, tcph, sizeof(struct click_tcp));
    	hdr->update();
     	return hdr;
    }

    static TransportHeaderEncap* MakeTCPHeader(click_tcp *tcph, u_char *opt, unsigned optlen) {
		assert(tcph != NULL);

    	TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_STREAM);

		hdr->_tcphdr = (struct click_tcp*)malloc(sizeof(struct click_tcp));
		memcpy(hdr->_tcphdr, tcph, sizeof(struct click_tcp));

		hdr->_options = (u_char*)malloc(optlen);
		memcpy(hdr->_options, opt, optlen);
		hdr->_optlen = optlen;

    	hdr->update();
     	return hdr;
    }

	static TransportHeaderEncap* MakeDGRAMHeader(uint16_t /* length */) {
		TransportHeaderEncap* hdr = new TransportHeaderEncap(SOCK_DGRAM);
		return hdr;
	}

	~TransportHeaderEncap() {
		if (_tcphdr) {
			delete _tcphdr;
			_tcphdr = NULL;
		}
		if (_options) {
			delete _options;
			_options = NULL;
		}
	}

	void update();

private:
	uint8_t _type;
	struct click_tcp *_tcphdr;
	u_char *_options;
	unsigned _optlen;

// FIXME: these need to be integrated into the new streaming transport
//    static TransportHeaderEncap* MakeMIGRATEHeader(uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
//                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::MIGRATE, seq_num, ack_num, length, recv_window); };
//
//    static TransportHeaderEncap* MakeMIGRATEACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
//                        { return new TransportHeaderEncap(TransportHeader::XSOCK_STREAM, TransportHeader::MIGRATEACK, seq_num, ack_num, length, recv_window); };
};

CLICK_ENDDECLS
#endif
