// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaStreamHeader.cc" -*-
#ifndef CLICK_STREAMHEADER_HH
#define CLICK_STREAMHEADER_HH

#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiaheader.hh>
#include <clicknet/tcp.h>

#include <clicknet/xtcp.h>

class StreamHeader
{
public:
	//StreamHeader(const StreamHeader& r)  { _hdr = r._hdr; };
	//StreamHeader(const struct xtcp* hdr) { _hdr = hdr; };

	// read from packet p->network_header() should point to XIA header
	StreamHeader(const Packet* p) {
		XIAHeader xh = XIAHeader(p);
		const struct xtcp *temp_hdr(reinterpret_cast<const struct xtcp*>(xh.next_header()));

		_plen = xh.plen();	// get the XIA Header's total payload size

		if (xh.nxt() == CLICK_XIA_NXT_XSTREAM) {
			_hdr = reinterpret_cast<const struct xtcp *>(xh.next_header());
			_plen -= hlen();	// remove our header's size from the payload sizee

		} else {
			uint32_t len = temp_hdr->th_off << 2;
			_plen -= len;

			while (temp_hdr->th_nxt != CLICK_XIA_NXT_XSTREAM && temp_hdr->th_nxt != CLICK_XIA_NXT_DATA) {
				// walk the header chain
				temp_hdr = (xtcp*)((char *)temp_hdr + len);				// get next header
				len = temp_hdr->th_off << 2;	// get size of next header
				_plen -= len;					// adjust payload size
			}

			if (temp_hdr->th_nxt == CLICK_XIA_NXT_XSTREAM) {
				_hdr = (xtcp*)((char *)temp_hdr + len);
				len = _hdr->th_off << 2;
				_plen -= len;
			} else {
				// something horrible happened and we're gonna crash
				click_chatter("Header %d not found!", CLICK_XIA_NXT_XSTREAM);
				_hdr = NULL;
				_plen = 0;
			}
		}

		assert (valid());
	};

    inline const struct xtcp* header() const { return _hdr; };
	inline bool valid()                   { return _hdr != NULL; };
	inline uint8_t nxt() const            { return _hdr->th_nxt; };
	inline uint16_t hlen() const          { return _hdr->th_off << 2; };
	inline uint32_t seq_num() const       { return ntohl(_hdr->th_seq); };
	inline uint32_t ack_num() const       { return ntohl(_hdr->th_ack); };
	inline uint32_t recv_window() const   { return ntohl(_hdr->th_win); };
	inline uint16_t flags() const         { return ntohs(_hdr->th_flags); };
	inline const u_char* tcpopt() const   { return reinterpret_cast<const uint8_t*>(_hdr) + sizeof(struct xtcp); };
	inline const uint8_t* payload() const { return reinterpret_cast<const uint8_t*>(_hdr) + hlen(); };
	inline uint32_t plen()                { return _plen; };

private:
	const struct xtcp *_hdr;
	uint32_t _plen;
};


class StreamHeaderEncap
{
public:
    StreamHeaderEncap() {
		_hdr = (struct xtcp*)calloc(1, sizeof(struct xtcp) + XTCP_OPTIONS_MAX);
		_hdr->th_nxt = CLICK_XIA_NXT_DATA;
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
			free(_hdr);
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
