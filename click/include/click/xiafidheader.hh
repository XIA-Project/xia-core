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
		const struct xfid *temp_hdr(reinterpret_cast<const struct xfid*>(xh.next_header()));

		if (xh.nxt() == CLICK_XIA_NXT_FID) {
			_hdr = reinterpret_cast<const struct xfid *>(xh.next_header());

		} else {
			click_chatter("Looking for Header of type %d\n", CLICK_XIA_NXT_FID);
			click_chatter("Next = %d\n", temp_hdr->th_nxt);
			click_chatter("temp_hdr = %p\n", temp_hdr);

			while (temp_hdr->th_nxt != CLICK_XIA_NXT_FID && temp_hdr->th_nxt != CLICK_XIA_NXT_DATA) {

				// walk the header chain
				temp_hdr += (temp_hdr->th_off << 2);
				click_chatter("Next = %d\n", temp_hdr->th_nxt);
			}

			if (temp_hdr->th_nxt == CLICK_XIA_NXT_FID) {
				_hdr = (temp_hdr + (temp_hdr->th_off << 2));
			} else {
				// something horrible happened and we're gonna crash
				click_chatter("Header %d not found!", CLICK_XIA_NXT_FID);
				_hdr = NULL;
			}
		}

		assert (valid());
	};

    inline const struct xfid* header() const { return _hdr; };
	inline bool valid()            { return _hdr != NULL && _hdr->th_nxt != CLICK_XIA_NXT_DATA; };
	inline uint8_t nxt() const     { return _hdr->th_nxt; };
	inline uint8_t hlen() const    { return _hdr->th_off << 2; };
	inline uint32_t seqnum() const { return _hdr->th_sequence; };

private:
	const struct xfid *_hdr;
};


class FIDHeaderEncap
{
public:
    FIDHeaderEncap(uint32_t next, uint32_t seq = 0) {
		_hdr = (struct xfid*)calloc(1, sizeof(struct xfid));
		_hdr->th_nxt = next;
		_hdr->th_off = sizeof(struct xfid) >> 2;
		_hdr->th_sequence = seq;
	}

	~FIDHeaderEncap() {
		if (_hdr) {
			delete _hdr;
		}
	}

	// encapsulate the given packet with a FID header
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
