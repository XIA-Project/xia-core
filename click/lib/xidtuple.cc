// -*- c-basic-offset: 4; related-file-name: "../include/click/xidtuple.hh" -*-
/*
 * xidtuple.{cc,hh} -- XIDtuple class
 *
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
#include <click/xidtuple.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#include <click/standard/xiaxidinfo.hh>
CLICK_DECLS


XIDtuple::XIDtuple()
{
    memset(&_a, 0, sizeof(_a));
	memset(&_b, 0, sizeof(_b));
    memset(&_c, 0, sizeof(_c));
}

XIDtuple::XIDtuple(const XID& a, const XID& b, const XID& c)
{
    _a = a;
	_b = b;
    _c = c;
}

void
XIDtuple::set_a(const XID& a)
{
    _a = a;
}

void
XIDtuple::set_b(const XID& b)
{
    _b = b;
}

void
XIDtuple::set_c(const XID& c)
{
    _c = c;
}

void
XIDtuple::dump()
{
	click_chatter("XIDtuple:%s %s %s", _a.unparse().c_str(), _b.unparse().c_str(), _c.unparse().c_str());
}


CLICK_ENDDECLS
