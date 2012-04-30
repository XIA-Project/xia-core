#ifndef CLICK_XIAPINGSOURCE_HH
#define CLICK_XIAPINGSOURCE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

XIAPingSource(SRC, DST [, I<keywords> INTERVAL])

=s xia

sends XIA ping requests from SRC to DST.

=d

Sends XIA ping packets with SRC as source and DST as destination.
DST will be updated to PONG messages's source (migration support).

=e

XIAPingSource(RE AD1 HID1, RE AD0 RHID0 HID0)

*/

class XIAPingSource : public Element { public:

    XIAPingSource();
    ~XIAPingSource();
  
    const char *class_name() const		{ return "XIAPingSource"; }
    const char *port_count() const		{ return "0-1/1"; }
    const char *processing() const		{ return "h/l"; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
  
    Packet *pull(int);
    void push(int, Packet *);
  
  private:
    Timer _timer;
  
    XIAPath _src_path;
    XIAPath _dst_path;
    uint32_t _count;
    uint32_t _print_every;
};

CLICK_ENDDECLS
#endif
