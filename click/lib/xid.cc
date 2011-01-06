// -*- c-basic-offset: 4; related-file-name: "../include/click/xid.hh" -*-
/*
 * xid.{cc,hh} -- XID class
 * Dongsu Han 
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/xid.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#include <click/standard/xiaxidinfo.hh>
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


/** @brief Return if the addresses are the same. */
bool
XID::operator==(const XID& rhs) const
{
    return memcmp(&_xid, &rhs._xid, sizeof(_xid)) == 0;
}

/** @brief Return if the addresses are different. */
bool
XID::operator!=(const XID& rhs) const
{
    return memcmp(&_xid, &rhs._xid, sizeof(_xid)) != 0;
}

void
XID::parse(const String& str)
{
    if (!cp_xid(str, this))
        _xid.type = CLICK_XIA_XID_TYPE_UNDEF;
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
    switch (_xid.type) {
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
        default:
           c += sprintf(c, "%02x", _xid.type);
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


CLICK_ENDDECLS
