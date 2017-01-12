// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaFIDHeader.cc" -*-
#ifndef CLICK_FIDHEADER_HH
#define CLICK_FIDHEADER_HH

#include <time.h>
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
	uint32_t th_tstamp;
};
#pragma pack(pop)

class FIDHeader
{
public:
	//FIDHeader(const FIDHeader& r)  { _hdr = r._hdr; };
	//FIDHeader(const struct xfid* hdr) { _hdr = hdr; };

	// read from packet p->network_header() should point to XIA header
	FIDHeader(const Packet* p) {
		XIAHeader xh = XIAHeader(p);
		const struct xfid *temp_hdr(reinterpret_cast<const struct xfid*>(xh.next_header()));

		_plen = xh.plen();	// get the XIA Header's total payload size

		if (xh.nxt() == CLICK_XIA_NXT_FID) {
			_hdr = reinterpret_cast<const struct xfid *>(xh.next_header());
			_plen -= hlen();	// remove our header's size from the payload sizee

		} else {
			uint32_t len = temp_hdr->th_off << 2;
			_plen -= len;

			while (temp_hdr->th_nxt != CLICK_XIA_NXT_FID && temp_hdr->th_nxt != CLICK_XIA_NXT_DATA) {
				// walk the header chain
				temp_hdr = (xfid*)((char *)temp_hdr + len);				// get next header
				len = temp_hdr->th_off << 2;	// get size of next header
				_plen -= len;					// adjust payload size
			}

			if (temp_hdr->th_nxt == CLICK_XIA_NXT_FID) {
				_hdr = (xfid*)((char *)temp_hdr + len);
				len = _hdr->th_off << 2;
				_plen -= len;
			} else {
				// something horrible happened and we're gonna crash
				click_chatter("Header %d not found!", CLICK_XIA_NXT_XDGRAM);
				_hdr = NULL;
				_plen = 0;
			}
		}

		assert (valid());
	};

    inline const struct xfid* header() const { return _hdr; };
	inline bool valid()            { return _hdr != NULL && _hdr->th_nxt != CLICK_XIA_NXT_DATA; };
	inline uint8_t nxt() const     { return _hdr->th_nxt; };
	inline uint8_t hlen() const    { return _hdr->th_off << 2; };
	inline uint32_t tstamp() const { return ntohl(_hdr->th_tstamp); };
	inline uint32_t seqnum() const { return ntohl(_hdr->th_sequence); };
	inline uint32_t plen()         { return _plen; };

private:
	const struct xfid *_hdr;
	uint32_t _plen;
};


class FIDHeaderEncap
{
public:
    FIDHeaderEncap(uint32_t next, uint32_t seq = 0) {
		_hdr = (struct xfid*)calloc(1, sizeof(struct xfid));
		_hdr->th_nxt = next;
		_hdr->th_off = sizeof(struct xfid) >> 2;
		_hdr->th_sequence = htonl(seq);
		_hdr->th_tstamp = htonl(time(NULL) & 0xffffffff);
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

	inline void seqnum(uint32_t s) { _hdr->th_sequence = htonl(s); };
	inline size_t hlen() const     { return _hdr->th_off << 2; };
	inline uint32_t tstamp() const { return ntohl(_hdr->th_tstamp); };

protected:
	struct xfid *_hdr;
};


CLICK_ENDDECLS
#endif
