#ifndef CLICK_XIDINFO_HH
#define CLICK_XIDINFO_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
CLICK_DECLS

/*
=c

XIDInfo(NAME0 XID0, ...)

=s xia

registers a friendly name for a XID

=d

Enables the user to use a name for a XID in configuration files and outputs.
To register a name, list pairs of a name and a XID.
Registered names are globally visible in Click.

=e

XIDAddressInfo(AD0 AD:000102030405060708090a0b0c0d0e0f10111213,
               CID0 CID:000102030405060708090a0b0c0d0e0f10111213)

=h src read/write

Returns or sets the SRC parameter.

=h dst read/write

Returns or sets the DST parameter.

=a

AddressInfo */

class XIDInfo : public Element { public:

  XIDInfo();
  ~XIDInfo();

  const char *class_name() const		{ return "XIDInfo"; }

  int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
  int configure(Vector<String> &, ErrorHandler *);

  static bool query_xid(const String& s, struct click_xia_xid* store, const Element *e);
  static String revquery_xid(const struct click_xia_xid* store, const Element *e);

};

CLICK_ENDDECLS
#endif

