// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaFIDHeader.cc" -*-
#ifndef CLICK_FIDHEADER_HH
#define CLICK_FIDHEADER_HH

#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiaheader.hh>

#pragma pack(push)
#pragma pack(1)
struct xfid {
	uint8_t  th_nxt;
	uint8_t  th_off;
	uint16_t th_padding;	// pad it out to 4 bytes
	uint32_t th_sequence;
};
#pragma pack(pop)

class FIDHeader
{
public:
	FIDHeader(const FIDHeader& r)  { _hdr = r._hdr; };
	FIDHeader(const struct xfid* hdr) { _hdr = hdr; };

	// read from packet p->network_header() should point to XIA header
	FIDHeader(const Packet* p) {
		XIAHeader xh = XIAHeader(p);
		assert(xh.nxt() == CLICK_XIA_NXT_FID);

		if (xh.nxt() == CLICK_XIA_NXT_FID) {
			_hdr = reinterpret_cast<const struct xfid*>(xh.next_header());
		} else {
			_hdr = NULL;
		}
	};

    inline const struct xfid* header() const { return _hdr; };
	inline uint8_t nxt() const     { return _hdr->th_nxt; };
	inline uint8_t hlen() const    { return _hdr->th_off << 2; };
	inline uint32_t seqnum() const { return _hdr->th_sequence; };

private:
	const struct xfid *_hdr;
};


class FIDHeaderEncap
{
public:
    FIDHeaderEncap(uint32_t seq = 0) {
		_hdr = (struct xfid*)calloc(1, sizeof(struct xfid));
		_hdr->th_nxt = CLICK_XIA_NXT_DATA;
		_hdr->th_off = sizeof(struct xfid) >> 2;
		_hdr->th_sequence = seq;
	}

    static FIDHeaderEncap* MakeFidHeader(xfid *dgh) {
		assert(dgh != NULL);

    	FIDHeaderEncap* h = new FIDHeaderEncap();

		memcpy(h->_hdr, dgh, sizeof(struct xfid));
		h->_hdr->th_off = sizeof(struct xfid) >> 2;
     	return h;
    }

	~FIDHeaderEncap() {
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

	inline void seqnum(uint32_t s) { _hdr->th_sequence = s; };
	inline size_t hlen() const { return _hdr->th_off << 2; };

protected:
	struct xfid *_hdr;
};


CLICK_ENDDECLS
#endif
