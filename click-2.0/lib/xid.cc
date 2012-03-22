// -*- c-basic-offset: 4; related-file-name: "../include/click/xid.hh" -*-
/*
 * xid.{cc,hh} -- XID class
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/xid.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#include <click/standard/xiaxidinfo.hh>
#include <click/args.hh>
CLICK_DECLS

/** @file xid.hh
 * @brief XID addresses.
 */

/** @class XID
    @brief XID helper class 

    The XID type represents an XID address. It
    provides methods for unparsing IP addresses
    into ASCII form. */

XID::XID()
{
    memset(&_xid, 0, sizeof(_xid));
}

XID::XID(const String &str)
{
    parse(str);
}

XID&
XID::operator=(const XID& rhs)
{
    _xid = rhs._xid;
    return *this;
}

XID&
XID::operator=(const struct click_xia_xid& rhs)
{
    _xid = rhs;
    return *this;
}

void
XID::parse(const String& str)
{
    if (!cp_xid(str, this))
        _xid.type = htonl(CLICK_XIA_XID_TYPE_UNDEF);
}

/** @brief Unparses this address into a String.

    Maintains the invariant that,
    for an XID @a a, XID(@a a.unparse()) == @a a. */
String
XID::unparse() const
{
    const unsigned char *p = _xid.id;
    char buf[48];
    char *c = buf;
    switch (ntohl(_xid.type)) {
        case CLICK_XIA_XID_TYPE_UNDEF:
           c += sprintf(c, "UNDEF");
           break;
        case CLICK_XIA_XID_TYPE_AD:
           c += sprintf(c, "AD");
           break;
        case CLICK_XIA_XID_TYPE_HID:
           c += sprintf(c, "HID");
           break;
        case CLICK_XIA_XID_TYPE_CID:
           c += sprintf(c, "CID");
           break;
        case CLICK_XIA_XID_TYPE_SID:
           c += sprintf(c, "SID");
           break;
        case CLICK_XIA_XID_TYPE_IP:
           c += sprintf(c, "IP");
	   break;
        default:
           c += sprintf(c, "%x", _xid.type);
    }
    c += sprintf(c, ":");
    for (size_t i = 0; i < sizeof(_xid.id); i++)
        c += sprintf(c, "%02x", p[i]);
    return String(buf);
}

String
XID::unparse_pretty(const Element* context) const
{
    String s;
#ifndef CLICK_TOOL
    if (context)
       s = XIAXIDInfo::revquery_xid(&_xid, context);
#else
    (void)context;
#endif
    if (s.length() != 0)
        return s;
    else
        return unparse();
}

StringAccum &
operator<<(StringAccum &sa, const XID& xid)
{
    return sa << xid.unparse();
}

bool
XIDArg::parse(const String &str, XID &value, const ArgContext & /* args */)
{

    XID xid_tmp;
    if (!cp_xid(str, &xid_tmp))
        (xid_tmp.xid()).type = htonl(CLICK_XIA_XID_TYPE_UNDEF);
        
    value = xid_tmp;
    
    return true;
}

bool
XIDArg::parse(const String &str, Args &args, unsigned char *value)
{
    XID *s = args.slot(*reinterpret_cast<XID *>(value));
    return s && parse(str, *s, args);
}

CLICK_ENDDECLS
