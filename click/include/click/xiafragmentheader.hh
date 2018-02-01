// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaFragmentHeader.cc" -*-
#ifndef CLICK_FIDHEADER_HH
#define CLICK_FIDHEADER_HH

#include <time.h>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiaheader.hh>

//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//|  Next Header  |     Offset    |      Fragment Offset    |Res|M|
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//|                         Identification                        |
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

#pragma pack(push)
#pragma pack(1)
struct xfrag {
    uint8_t  fh_nxt;		// next header type
    uint8_t  fh_off;		// size of the header in 4 byte words
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
	unsigned fh_more : 1;	// more fragments coming if set to 1
    unsigned fh_res : 2;	// reserved
    unsigned fh_offset: 13;	// staart of fragment in 8 byte words
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned fh_offset: 13;	// staart of fragment in 8 byte words
    unsigned fh_res : 2;	// reserved
	unsigned fh_more : 1;	// more fragments coming if set to 1
#else
#   error "unknown byte order"
#endif
	unint32_t fh_id;       // unique fragment id
};
#pragma pack(pop)

class FragmentHeader
{
public:
    //FragmentHeader(const FragmentHeader& r)  { _hdr = r._hdr; };
    //FragmentHeader(const struct xfrag* hdr) { _hdr = hdr; };

    // read from packet p->network_header() should point to XIA header
    FragmentHeader(const Packet* p) {
        XIAHeader xh = XIAHeader(p);
        const struct xfrag *temp_hdr(reinterpret_cast<const struct xfrag*>(xh.next_header()));

        _plen = xh.plen();    // get the XIA Header's total payload size

        if (xh.nxt() == CLICK_XIA_NXT_FID) {
            _hdr = reinterpret_cast<const struct xfrag *>(xh.next_header());
            _plen -= hlen();    // remove our header's size from the payload sizee

        } else {
            uint32_t len = temp_hdr->fh_off << 2;
            _plen -= len;

            while (temp_hdr->fh_nxt != CLICK_XIA_NXT_FID && temp_hdr->fh_nxt != CLICK_XIA_NXT_DATA) {
                // walk the header chain
                temp_hdr = (xfrag*)((char *)temp_hdr + len);                // get next header
                len = temp_hdr->fh_off << 2;    // get size of next header
                _plen -= len;                    // adjust payload size
            }

            if (temp_hdr->fh_nxt == CLICK_XIA_NXT_FID) {
                _hdr = (xfrag*)((char *)temp_hdr + len);
                len = _hdr->fh_off << 2;
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

    inline const struct xfrag* header() const { return _hdr; };
    inline bool valid()            { return _hdr != NULL && _hdr->fh_nxt != CLICK_XIA_NXT_DATA; };
    inline uint8_t nxt() const     { return _hdr->fh_nxt; };
    inline uint8_t hlen() const    { return _hdr->fh_off << 2; };
    inline uint32_t tstamp() const { return ntohl(_hdr->fh_tstamp); };
    inline uint32_t seqnum() const { return ntohl(_hdr->fh_sequence); };
    inline uint32_t plen()         { return _plen; };

private:
    const struct xfrag *_hdr;
    uint32_t _plen;
};


class FragmentHeaderEncap
{
public:
    FragmentHeaderEncap(uint32_t next, uint32_t seq = 0) {
        _hdr = (struct xfrag*)calloc(1, sizeof(struct xfrag));
        _hdr->fh_nxt = next;
        _hdr->fh_off = sizeof(struct xfrag) >> 2;
        _hdr->fh_sequence = htonl(seq);
        _hdr->fh_tstamp = htonl(time(NULL) & 0xffffffff);
    }

    ~FragmentHeaderEncap() {
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

    inline void seqnum(uint32_t s) { _hdr->fh_sequence = htonl(s); };
    inline size_t hlen() const     { return _hdr->fh_off << 2; };
    inline uint32_t tstamp() const { return ntohl(_hdr->fh_tstamp); };

protected:
    struct xfrag *_hdr;
};


CLICK_ENDDECLS
#endif
