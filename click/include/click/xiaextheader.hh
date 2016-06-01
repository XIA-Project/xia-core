// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaextheader.cc" -*-
#ifndef CLICK_XIAEXTHEADER_HH
#define CLICK_XIAEXTHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/xiaheader.hh>

CLICK_DECLS

// A read-only helper class for XIA extension headers.
class XIAGenericExtHeader { public:
    XIAGenericExtHeader(const XIAGenericExtHeader& r); // copy constructor

    XIAGenericExtHeader(const struct click_xia_ext* hdr);
    XIAGenericExtHeader(const Packet* p);  // read from packet p->network_header() should point to XIA header

    inline const struct click_xia_ext* hdr() const;

    inline const uint8_t& nxt() const;      // next header type
    inline const uint8_t& hlen() const;     // header length
	inline const uint8_t& type() const;		// FIXME: TEMPORARY! transport type
    inline const uint8_t* payload() const;  // payload

protected:
    const struct click_xia_ext* _hdr;

    inline XIAGenericExtHeader() : _hdr(NULL) { }  // for helping WritableXIAGenericExtHeader hide dangerous construction

    friend class WritableXIAGenericExtHeader;
};


// A partially writable helper class for XIA extension headers.
// Key-value data are not writable because it can change the header size;
// Use XIAGenericExtHeaderEncap to modify key-value data.
class WritableXIAGenericExtHeader : public XIAGenericExtHeader { public:
    WritableXIAGenericExtHeader(const WritableXIAGenericExtHeader& r);

    WritableXIAGenericExtHeader(struct click_xia_ext* hdr);
    WritableXIAGenericExtHeader(WritablePacket* p);

    inline struct click_xia_ext* hdr();

    inline void set_nxt(uint8_t nxt);       // set next header type
	inline void set_type(uint8_t type);		// FIXME: TEMPORARY!
    inline uint8_t* payload();              // settable payload

private:
    // hides all dangerous construction
    WritableXIAGenericExtHeader(const XIAGenericExtHeader&) { assert(false); }
    WritableXIAGenericExtHeader(const struct click_xia_ext*) { assert(false); }
    WritableXIAGenericExtHeader(const Packet*) { assert(false); }
};


// An XIA extension header encapsulation helper.
class XIAGenericExtHeaderEncap { public:
    XIAGenericExtHeaderEncap();
    XIAGenericExtHeaderEncap(const XIAGenericExtHeaderEncap& r);
    virtual ~XIAGenericExtHeaderEncap();

    XIAGenericExtHeaderEncap(const XIAGenericExtHeader& r);

    const struct click_xia_ext* hdr() const;
    struct click_xia_ext* hdr();

    const uint8_t& hlen() const;	// header length (need to manually call update() first)
    void set_nxt(uint8_t nxt);		// set next header type
	void set_type(uint8_t type);	// FIXME: TEMPORARY!

    // encapsulate the given packet with an XIA extension header. (need to manually call update() first)
    WritablePacket* encap(Packet* p_in) const;

protected:
    struct click_xia_ext* _hdr;
};


inline const struct click_xia_ext*
XIAGenericExtHeader::hdr() const
{
    return _hdr;
}

inline const uint8_t&
XIAGenericExtHeader::nxt() const
{
    return _hdr->nxt;
}

inline const uint8_t&
XIAGenericExtHeader::hlen() const
{
    return _hdr->hlen;
}

inline const uint8_t&
XIAGenericExtHeader::type() const
{
    return _hdr->type;
}

inline const uint8_t*
XIAGenericExtHeader::payload() const
{
    return reinterpret_cast<const uint8_t*>(_hdr) + hlen();
}

inline struct click_xia_ext*
WritableXIAGenericExtHeader::hdr()
{
    return const_cast<struct click_xia_ext*>(_hdr);
}

inline void
WritableXIAGenericExtHeader::set_nxt(uint8_t nxt)
{
    const_cast<struct click_xia_ext*>(_hdr)->nxt = nxt;
}

inline void
WritableXIAGenericExtHeader::set_type(uint8_t type)
{
    const_cast<struct click_xia_ext*>(_hdr)->type = type;
}

inline uint8_t*
WritableXIAGenericExtHeader::payload()
{
    return const_cast<uint8_t*>(this->XIAGenericExtHeader::payload());
}

CLICK_ENDDECLS
#endif
