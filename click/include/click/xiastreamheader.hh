// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaStreamHeader.cc" -*-
#ifndef CLICK_STREAMHEADER_HH
#define CLICK_STREAMHEADER_HH

#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiaheader.hh>
#include <clicknet/tcp.h>

// FIXME: these need to be integrated into the new streaming transport somehow
//    static StreamHeaderEncap* MakeMIGRATEHeader(uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
//                        { return new StreamHeaderEncap(StreamHeader::XSOCK_STREAM, StreamHeader::MIGRATE, seq_num, ack_num, length, recv_window); };
//
//    static StreamHeaderEncap* MakeMIGRATEACKHeader( uint32_t seq_num, uint32_t ack_num, uint16_t length, uint32_t recv_window )
//                        { return new StreamHeaderEncap(StreamHeader::XSOCK_STREAM, StreamHeader::MIGRATEACK, seq_num, ack_num, length, recv_window); };

#include <clicknet/xtcp.h>

class StreamHeader
{
public:
	StreamHeader(const StreamHeader& r)  { _hdr = r._hdr; };
	StreamHeader(const struct xtcp* hdr) { _hdr = hdr; };

	// read from packet p->network_header() should point to XIA header
	StreamHeader(const Packet* p) {
		XIAHeader xh = XIAHeader(p);
		assert(xh.nxt() == CLICK_XIA_NXT_XTCP);

		if (xh.nxt() == CLICK_XIA_NXT_XTCP) {
			_hdr = reinterpret_cast<const struct xtcp*>(xh.next_header());
		} else {
			_hdr = NULL;
		}
	};

    inline const struct xtcp* header() const { return _hdr; };
	inline uint8_t nxt() const            { return _hdr->th_nxt; };
	inline uint8_t hlen() const           { return _hdr->th_off << 2; };
	inline uint32_t seq_num() const       { return ntohl(_hdr->th_seq); };
	inline uint32_t ack_num() const       { return ntohl(_hdr->th_ack); };
	inline uint32_t recv_window() const   { return ntohl(_hdr->th_win); };
	inline uint16_t flags() const         { return ntohs(_hdr->th_flags); };
	inline const u_char* tcpopt() const   { return reinterpret_cast<const uint8_t*>(_hdr) + sizeof(struct xtcp); };
	inline const uint8_t* payload() const { return reinterpret_cast<const uint8_t*>(_hdr) + hlen(); };

private:
	const struct xtcp *_hdr;
};


class StreamHeaderEncap
{
public:
    StreamHeaderEncap() {
		_hdr = (struct xtcp*)calloc(1, sizeof(struct xtcp) + XTCP_OPTIONS_MAX);
		_hdr->th_nxt = CLICK_XIA_NXT_NO;
	}

    static StreamHeaderEncap* MakeTCPHeader(xtcp *tcph) {
		assert(tcph != NULL);

    	StreamHeaderEncap* h = new StreamHeaderEncap();
		// if th_off is non 0 copy the options too
		memcpy(h->_hdr, tcph, tcph->th_off << 2);
     	return h;
    }

    static StreamHeaderEncap* MakeTCPHeader(xtcp *tcph, u_char *opt, unsigned optlen) {
		assert(tcph != NULL);

    	StreamHeaderEncap* h = new StreamHeaderEncap();

		memcpy(h->_hdr, tcph, sizeof(struct xtcp));
		u_char *o = reinterpret_cast<u_char*>(h->_hdr) + sizeof(struct xtcp);
		memcpy(o, opt, optlen);
		h->_hdr->th_off = (sizeof(struct xtcp) + optlen) >> 2;
     	return h;
    }

	~StreamHeaderEncap() {
		if (_hdr) {
			delete _hdr;
		}
	}

	// encapsulate the given packet with an Stream header
	WritablePacket* encap(Packet* p_in) const
	{
	    size_t len = hlen();
	    WritablePacket* p = p_in->push(len);
	    if (!p)
	        return NULL;

	    memcpy(p->data(), _hdr, hlen());  // copy the header
	    return p;
	}

	size_t hlen() const { return _hdr->th_off << 2; };

protected:
	struct xtcp *_hdr;
};


CLICK_ENDDECLS
#endif
