// -*- c-basic-offset: 4; related-file-name: "../../lib/xid.cc" -*-
#ifndef CLICK_XID_HH
#define CLICK_XID_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
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
    
    inline unsigned char* data();
    inline const unsigned char* data() const;
    
    
    inline operator struct click_xia_xid() const;

    inline uint32_t hashcode() const;

    inline bool operator==(const XID&) const;
    inline bool operator!=(const XID&) const;

    XID& operator=(const XID&);
    XID& operator=(const struct click_xia_xid&);

    void parse(const String& str);
    String unparse() const;
    String unparse_pretty(const Element* context = NULL) const;

	bool valid() { return (ntohl(_xid.type) != CLICK_XIA_XID_TYPE_UNDEF); };

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
// this is disabled to guarantee a correct _hash value

inline struct click_xia_xid&
XID::xid() 
{
    return _xid;
}

/** @brief Return a pointer to the address data. */
inline const unsigned char*
XID::data() const
{
    return reinterpret_cast<const unsigned char*>(&_xid);
}

/** @brief Return a pointer to the address data. */
inline unsigned char*
XID::data()
{
    return reinterpret_cast<unsigned char*>(&_xid);
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
    return reinterpret_cast<const uint32_t*>(&_xid)[4]; // pick some word in the middle
}

/** @brief Return if the addresses are the same. */
inline bool
XID::operator==(const XID& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&_xid)[0] == reinterpret_cast<const uint32_t*>(&rhs._xid)[0] &&
           reinterpret_cast<const uint32_t*>(&_xid)[1] == reinterpret_cast<const uint32_t*>(&rhs._xid)[1] &&
           reinterpret_cast<const uint32_t*>(&_xid)[2] == reinterpret_cast<const uint32_t*>(&rhs._xid)[2] &&
           reinterpret_cast<const uint32_t*>(&_xid)[3] == reinterpret_cast<const uint32_t*>(&rhs._xid)[3] &&
           reinterpret_cast<const uint32_t*>(&_xid)[4] == reinterpret_cast<const uint32_t*>(&rhs._xid)[4] &&
           reinterpret_cast<const uint32_t*>(&_xid)[5] == reinterpret_cast<const uint32_t*>(&rhs._xid)[5];
}

/** @brief Return if the addresses are different. */
inline bool
XID::operator!=(const XID& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&_xid)[0] != reinterpret_cast<const uint32_t*>(&rhs._xid)[0] ||
           reinterpret_cast<const uint32_t*>(&_xid)[1] != reinterpret_cast<const uint32_t*>(&rhs._xid)[1] ||
           reinterpret_cast<const uint32_t*>(&_xid)[2] != reinterpret_cast<const uint32_t*>(&rhs._xid)[2] ||
           reinterpret_cast<const uint32_t*>(&_xid)[3] != reinterpret_cast<const uint32_t*>(&rhs._xid)[3] ||
           reinterpret_cast<const uint32_t*>(&_xid)[4] != reinterpret_cast<const uint32_t*>(&rhs._xid)[4] ||
           reinterpret_cast<const uint32_t*>(&_xid)[5] != reinterpret_cast<const uint32_t*>(&rhs._xid)[5];
}


class ArgContext;
class Args;
extern const ArgContext blank_args;

/** @class XIDArg
  @brief Parser class for XIA addresses. */
  
struct XIDArg {
    static bool parse(const String &str, XID &value, const ArgContext &args = blank_args);
    //static bool parse(const String &str, unsigned char *value, const ArgContext &args = blank_args) {
	//return parse(str, *reinterpret_cast<EtherAddress *>(value), args);
    //}
    static bool parse(const String &str, Args &args, unsigned char *value);
};

template<> struct DefaultArg<XID> : public XIDArg {};


CLICK_ENDDECLS
#endif
