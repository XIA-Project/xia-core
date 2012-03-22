#ifndef CLICK_XIAPINGUPDATE_HH
#define CLICK_XIAPINGUPDATE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

XIAPingUpdate(SRC, DST, NEW)

=s xia

sends an XIA PING UPDATE message.

=d

Sends an update message to change XIAPingSource's source path and XIAPingReponder's destination path with a new path

=e

XIAPingUpdate(RE AD0 RHID0, HID0, RE AD0 RHID0 HID0)

*/

class XIAPingUpdate : public Element { public:

    XIAPingUpdate();
    ~XIAPingUpdate();
  
    const char *class_name() const		{ return "XIAPingUpdate"; }
    const char *port_count() const		{ return "0/1"; }
    const char *processing() const		{ return PULL; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
  
    Packet *pull(int);
    static WritablePacket *make_packet(const XIAPath& src_path, const XIAPath& dst_path, const XIAPath& new_path);
  
  private:
    XIAPath _src_path;
    XIAPath _dst_path;
    XIAPath _new_path;
};

CLICK_ENDDECLS
#endif
