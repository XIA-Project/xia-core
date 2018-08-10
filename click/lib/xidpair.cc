// -*- c-basic-offset: 4; related-file-name: "../include/click/xidpair.hh" -*-
/*
 * xidpair.{cc,hh} -- XIDpair class
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
#include <click/xidpair.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/integers.hh>
#include <click/standard/xiaxidinfo.hh>
CLICK_DECLS


XIDpair::XIDpair()
{
    memset(&src_xid, 0, sizeof(src_xid));
    memset(&dst_xid, 0, sizeof(dst_xid));
}

XIDpair::XIDpair(const XID& src, const XID& dst)
{
    src_xid = src;
    dst_xid = dst;
}

void
XIDpair::set_src(const XID& src)
{
    src_xid = src;
}

void
XIDpair::set_dst(const XID& dst)
{
    dst_xid = dst;
}

XID&
XIDpair::src()
{
	return src_xid;
}

XID&
XIDpair::dst()
{
	return dst_xid;
}

CLICK_ENDDECLS
