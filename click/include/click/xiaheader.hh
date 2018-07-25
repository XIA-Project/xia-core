// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaheader.cc" -*-
#ifndef CLICK_XIAHEADER_HH
#define CLICK_XIAHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/xiapath.hh>

CLICK_DECLS

// A read-only helper class for XIA headers.
class XIAHeader { public:
    inline XIAHeader(const XIAHeader& r);

    inline XIAHeader(const struct click_xia* hdr);
    inline XIAHeader(const Packet* p);

    static inline size_t hdr_size(uint8_t dsnode);  // header size with total dsnode nodes

    inline const struct click_xia* hdr() const;

    inline size_t hdr_size() const;         // header size

    inline const uint8_t& nxt() const;      // next header type

    inline uint16_t plen() const;           // payload length (in host byte order)

    inline const int8_t& last() const;      // last visited node

    inline const uint8_t& hlim() const;     // hop limit

    XIAPath dst_path() const;               // destination path (expensive call)
    XIAPath src_path() const;               // source path (expensive call)

    inline const uint8_t* next_header() const;  // next header

    const uint8_t* payload() const;         // payload (expensive call; need to traverse extension headers)

private:
    const struct click_xia* _hdr;

    inline XIAHeader() : _hdr(NULL) { }     // for helping WritableXIAHeader hide dangerous construction

    friend class WritableXIAHeader;
};


// A partially writable helper class for XIA headers.
// Paths are not writable because it can change the header size;
// Use XIAHeaderEncap to change new paths.
class WritableXIAHeader : public XIAHeader { public:
    inline WritableXIAHeader(const WritableXIAHeader& r);

    inline WritableXIAHeader(struct click_xia* hdr);
    inline WritableXIAHeader(WritablePacket* p);

    inline struct click_xia* hdr();

    inline void set_nxt(uint8_t nxt);           // set next header type

    inline void set_plen(uint16_t plen);        // set payload length (in host byte order)

    inline void set_last(int8_t last);          // set last visited node

    inline void set_hlim(uint8_t hlim);         // set hop limit

    inline uint8_t* payload();                  // settable payload

private:
    // hides all dangerous construction
    WritableXIAHeader(const XIAHeader&) { assert(false); }
    WritableXIAHeader(const struct click_xia*) { assert(false); }
    WritableXIAHeader(const Packet*) { assert(false); }
};


// An XIA header encapsulation helper.
class XIAHeaderEncap { public:
    XIAHeaderEncap();
    XIAHeaderEncap(const XIAHeaderEncap& r);
    ~XIAHeaderEncap();

    XIAHeaderEncap(const XIAHeader& r);

    const struct click_xia* hdr() const;
    struct click_xia* hdr();

    size_t hdr_size() const;                    // header size

    void set_nxt(uint8_t nxt);                  // set next header type

    void set_plen(uint16_t plen);               // set payload length

    void set_last(int8_t last);                 // set last visited node

    void set_hlim(uint8_t hlim);                // set hop limit

    void set_dst_path(const XIAPath& path);     // set destination path
    void set_src_path(const XIAPath& path);     // set source path

    XIAPath& dst_path() { return _dst_path; } ;     // get destination path
    XIAPath& src_path() { return _src_path; } ;     // get source path

    // encapsulate the given packet with an XIA header.
    // update the payload length to the p_in->length() if adjust_plen is true
    // (i.e. manual set_plen(p_in->length()) invocation is unnecessary)
    WritablePacket* encap(Packet* p_in, bool adjust_plen = true) const;

	// replace the original header with a new one
	// NOTE: the last visited pointer remains unchanged
	WritablePacket* encap_replace(Packet *p_in) const;

    void update();

private:
    struct click_xia* _hdr;
    XIAPath _dst_path;
    XIAPath _src_path;
};


inline
XIAHeader::XIAHeader(const XIAHeader& r)
    : _hdr(r._hdr)
{
}

inline
XIAHeader::XIAHeader(const struct click_xia* hdr)
    : _hdr(hdr)
{
}

inline
XIAHeader::XIAHeader(const Packet* p)
    : _hdr(p->xia_header())
{
}

inline const struct click_xia*
XIAHeader::hdr() const
{
    return _hdr;
}

inline size_t
XIAHeader::hdr_size(uint8_t dsnode)
{
    return sizeof(struct click_xia) + sizeof(struct click_xia_xid_node) * dsnode;
}

inline size_t
XIAHeader::hdr_size() const
{
    return hdr_size(_hdr->dnode + _hdr->snode);
}

inline const uint8_t&
XIAHeader::nxt() const
{
    return _hdr->nxt;
}

inline uint16_t
XIAHeader::plen() const
{
    return ntohs(_hdr->plen);
}

inline const int8_t&
XIAHeader::last() const
{
    return _hdr->last;
}

inline const uint8_t&
XIAHeader::hlim() const
{
    return _hdr->hlim;
}

inline const uint8_t*
XIAHeader::next_header() const
{
    return reinterpret_cast<const uint8_t*>(_hdr) + hdr_size();
}

/* Returns layer 3 payload (this includes transport header) */
inline const uint8_t*
XIAHeader::payload() const
{
    return next_header();
}

inline
WritableXIAHeader::WritableXIAHeader(const WritableXIAHeader& r)
    : XIAHeader(r._hdr)
{
}

inline
WritableXIAHeader::WritableXIAHeader(struct click_xia* hdr)
    : XIAHeader(hdr)
{
}

inline
WritableXIAHeader::WritableXIAHeader(WritablePacket* p)
    : XIAHeader(p)
{
}

inline struct click_xia*
WritableXIAHeader::hdr()
{
    return const_cast<struct click_xia*>(_hdr);
}

inline void
WritableXIAHeader::set_nxt(uint8_t nxt)
{
    const_cast<struct click_xia*>(_hdr)->nxt = nxt;
}

inline void
WritableXIAHeader::set_plen(uint16_t plen)
{
    const_cast<struct click_xia*>(_hdr)->plen = htons(plen);
}

inline void
WritableXIAHeader::set_last(int8_t last)
{
    const_cast<struct click_xia*>(_hdr)->last = last;
}

inline void
WritableXIAHeader::set_hlim(uint8_t hlim)
{
    const_cast<struct click_xia*>(_hdr)->hlim = hlim;
}


inline uint8_t*
WritableXIAHeader::payload()
{
    return const_cast<uint8_t*>(this->XIAHeader::payload());
}

CLICK_ENDDECLS
#endif
