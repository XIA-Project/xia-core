#ifndef CLICK_XCMP_HH
#define CLICK_XCMP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
CLICK_DECLS

/*
=c

XCMP(SRC)

=s xia

Responds to various XCMP requests

=d

Responds to various XCMP requests

=e

XCMP(RE AD0 RHID0 HID0)

*/

class XCMP : public Element { public:

  XCMP();
  ~XCMP();

  const char *class_name() const		{ return "XCMP"; }
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void push(int, Packet *);

 private:

  u_short in_cksum(u_short *, int);
  XIAPath _src_path;
  //uint32_t _count;

  //bool _connected;
  //XIAPath _last_client;
};

CLICK_ENDDECLS
#endif
