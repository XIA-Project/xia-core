// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaDatagramHeader.cc" -*-
#ifndef CLICK_DATAGRAMHEADER_HH
#define CLICK_DATAGRAMHEADER_HH

#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiaheader.hh>

#pragma pack(push)
#pragma pack(1)
struct xdgram {
	uint8_t  th_nxt;
	uint8_t  th_off;
	uint16_t th_padding;	// pad it out to 4 bytes
	uint8_t  data[0];
};
#pragma pack(pop)

class DatagramHeader
{
public:
	//DatagramHeader(const DatagramHeader& r)  { _hdr = r._hdr; };
	//DatagramHeader(const struct xdgram* hdr) { _hdr = hdr; };

	// read from packet p->network_header() should point to XIA header
	DatagramHeader(const Packet* p) {
		XIAHeader xh = XIAHeader(p);
		const struct xdgram *temp_hdr(reinterpret_cast<const struct xdgram*>(xh.next_header()));

		_plen = xh.plen();	// get the XIA Header's total payload size

		if (xh.nxt() == CLICK_XIA_NXT_XDGRAM) {
			_hdr = reinterpret_cast<const struct xdgram *>(xh.next_header());
			_plen -= hlen();	// remove our header's size from the payload sizee

		} else {
			uint32_t len = temp_hdr->th_off << 2;
			_plen -= len;

			while (temp_hdr->th_nxt != CLICK_XIA_NXT_XDGRAM && temp_hdr->th_nxt != CLICK_XIA_NXT_DATA) {
				// walk the header chain
				temp_hdr = (xdgram*)((char *)temp_hdr + len);				// get next header
				len = temp_hdr->th_off << 2;	// get size of next header
				_plen -= len;					// adjust payload size
				click_chatter("Next = %d\n", temp_hdr->th_nxt);
			}

			if (temp_hdr->th_nxt == CLICK_XIA_NXT_XDGRAM) {
				_hdr = (xdgram*)((char *)temp_hdr + len);
				len = _hdr->th_off << 2;
				_plen -= len;
				click_chatter("len = %d nxt = %d XIA plen = %d DG pLen = %d", len, _hdr->th_nxt, xh.plen(), plen());
			} else {
				// something horrible happened and we're gonna crash
				click_chatter("Header %d not found!", CLICK_XIA_NXT_XDGRAM);
				_hdr = NULL;
				_plen = 0;
			}
		}

		assert (valid());
	};

    inline const struct xdgram* header() const { return _hdr; };
	inline bool valid()                   { return _hdr != NULL; };
	inline uint8_t nxt() const            { return _hdr->th_nxt; };
	inline uint8_t hlen() const           { return _hdr->th_off << 2; };
	inline const uint8_t* payload() const { return reinterpret_cast<const uint8_t*>(_hdr) + hlen(); };
	inline uint32_t plen()                { return _plen; };

private:
	const struct xdgram *_hdr;
	uint32_t _plen;
};


class DatagramHeaderEncap
{
public:
    DatagramHeaderEncap() {
		_hdr = (struct xdgram*)calloc(1, sizeof(struct xdgram));
		_hdr->th_nxt = CLICK_XIA_NXT_DATA;
		_hdr->th_off = sizeof(struct xdgram) >> 2;
	}

    static DatagramHeaderEncap* MakeDgramHeader(xdgram *dgh) {
		assert(dgh != NULL);

    	DatagramHeaderEncap* h = new DatagramHeaderEncap();

		memcpy(h->_hdr, dgh, sizeof(struct xdgram));
		h->_hdr->th_off = sizeof(struct xdgram) >> 2;
     	return h;
    }

	~DatagramHeaderEncap() {
		if (_hdr) {
			delete _hdr;
		}
	}

	// encapsulate the given packet with a Datagram header
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
	struct xdgram *_hdr;
};


CLICK_ENDDECLS
#endif
