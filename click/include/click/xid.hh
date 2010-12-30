// -*- c-basic-offset: 4; related-file-name: "../../lib/xid.cc" -*-
#ifndef CLICK_XID_HH
#define CLICK_XID_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <vector>
CLICK_DECLS
class StringAccum;
class Element;

class XID { public:
    XID();

    /** @brief Construct an XID from a struct click_xia_xid. */
    inline XID(const struct click_xia_xid& xid)
	: _xid(xid) {
    }

    XID(const String& str);

    inline const struct click_xia_xid& xid() const;
    inline struct click_xia_xid& xid();

    inline operator struct click_xia_xid() const;

    inline uint32_t hashcode() const;

    bool operator==(const XID&) const;
    bool operator!=(const XID&) const;

    void parse(const String& str);
    String unparse() const;
    String unparse_pretty(Element* context = NULL) const;

  private:
    struct click_xia_xid _xid;
};


/** @brief Return a struct click_xia_xid corresponding to the address. */
inline const struct click_xia_xid&
XID::xid() const
{
    return _xid;
}

/** @brief Return a struct click_xia_xid corresponding to the address. */
inline struct click_xia_xid&
XID::xid() 
{
    return _xid;
}

/** @brief Return a struct click_xia_xid corresponding to the address. */
inline
XID::operator struct click_xia_xid() const
{
    return xid();
}

StringAccum& operator<<(StringAccum&, const XID&);


/** @brief Hash function.
 * @return The hash value of this XID.
 *
 * returns the first 32 bit of XID. This hash function assumes that XIDs are generated cryptographically 
 */
inline uint32_t
XID::hashcode() const
{
    return String::hashcode(_xid.id, _xid.id + sizeof(_xid.id));
}

CLICK_ENDDECLS
#endif
