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
CLICK_DECLS

/** @file xid.hh
 * @brief XID addresses.
 */

/** @class XID
    @brief XID helper class 

    The XID type represents an XID address. It
    provides methods for unparsing IP addresses
    into ASCII form. */

XID::XID(const String &str)
{
    if (!cp_xid(str, this))
	_xid.type = UNDEFINED_TYPE;
}


/** @brief Unparses this address into a String.

    Maintains the invariant that,
    for an XID @a a, XID(@a a.unparse()) == @a a. */
String
XID::unparse() const
{
    const unsigned char *p =_xid.xid;
    char buf[46];
    char *c = buf;
    c+= sprintf(c, "%04x:", _xid.type);
    for (size_t i=0;i<sizeof(_xid.xid);i++)
        c+=sprintf(c, "%02x", p[i]);
    return String(buf);
}


StringAccum &
operator<<(StringAccum &sa, XID xid) 
{
    struct click_xid_v1 xid_s= xid.xid();

    const unsigned char *p =xid_s.xid;
    char buf[46];
    char *c = buf;
    switch (xid_s.type) {
        case UNDEFINED_TYPE:
           c+= sprintf(c, "UNDEF:");
           break;
        case AD_TYPE:
           c+= sprintf(c, "AD:");
           break;
        case HID_TYPE:
           c+= sprintf(c, "HID:");
           break;
        case CID_TYPE:
           c+= sprintf(c, "CID:");
           break;
        case SID_TYPE:
           c+= sprintf(c, "SID:");
           break;
        default:
           c+= sprintf(c, "%02x", xid_s.type);
    }
    for (size_t i=0;i<sizeof(xid_s.xid);i++)
        c+=sprintf(c, "%02x", p[i]);
    sa.append(buf, c-buf);
    return sa;
}

CLICK_ENDDECLS
