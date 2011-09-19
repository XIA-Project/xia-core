#ifndef CLICK_INSERTHASH_HH
#define CLICK_INSERTHASH_HH
#include <click/element.hh>
#include "xiafastpath.hh"

CLICK_DECLS
class XIAInsertHash : public Element { public:
    XIAInsertHash();
    ~XIAInsertHash();
    const char *class_name() const      { return "XIAInsertHash"; }
    const char *port_count() const      { return "1/1"; }
    const char *processing() const      { return AGNOSTIC; }
    Packet * simple_action( Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
   
    private:
    uint32_t _offset;
};


CLICK_ENDDECLS
#endif
