// -*- c-basic-offset: 4; related-file-name: "../../lib/xid.cc" -*-
#ifndef CLICK_XID_HH
#define CLICK_XID_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
CLICK_DECLS
class StringAccum;

class XID { public:
    /** @brief Construct an XID from a struct click_xid_v1. */
    inline XID(struct click_xid_v1& xid)
	: _xid(xid) {
    }

    XID(const String& s);

    inline struct click_xid_v1 xid() const;
    inline struct click_xid_v1& xid();
    inline uint32_t hashcode() const;

    // bool operator==(XID, XID);
    // bool operator!=(XID, XID);

    //inline XID& operator&=(XID);
    //inline XID& operator|=(XID);
    //inline XID& operator^=(XID);
    inline operator struct click_xid_v1() const;

    String unparse() const;

    inline String s() const;

  private:

    struct click_xid_v1 _xid;

};



/** @brief Return a struct click_xid_v1 corresponding to the address. */
inline struct click_xid_v1
XID::xid() const
{
    return _xid;
}

/** @brief Return a struct click_xid_v1 corresponding to the address. */
inline struct click_xid_v1&
XID::xid() 
{
    return _xid;
}

/** @brief Return a struct click_xid_v1 corresponding to the address. */
inline
XID::operator struct click_xid_v1() const
{
    return xid();
}

StringAccum& operator<<(StringAccum&, XID);


/** @brief Hash function.
 * @return The hash value of this XID.
 *
 * returns the first 32 bit of XID. This hash function assumes that XIDs are generated cryptographically 
 */
inline uint32_t
XID::hashcode() const
{
    return (uint32_t)(*_xid.xid);
}


CLICK_ENDDECLS
#endif
