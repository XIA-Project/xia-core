#ifndef CLICK_XIAOVERLAYFILTER_HH
#define CLICK_XIAOVERLAYFILTER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

XIAOverlayFilter

=s dst_port_annotation

sends packet stream to output based on IP annotation vs. XIA PAINT

=d

XIAOverlayFilter sends UDP/IP packets to output 0 and all other XIA
PAINT annotated packets to output 1.*/

class XIAOverlayFilter : public Element { public:

    XIAOverlayFilter();
    ~XIAOverlayFilter();

    const char *class_name() const		{ return "XIAOverlayFilter"; }
    const char *port_count() const		{ return "2/4"; }
    const char *processing() const		{ return PUSH; }

    //int configure(Vector<String> &conf, ErrorHandler *errh);

    void push(int, Packet *);

  private:

    uint8_t _anno;
};

CLICK_ENDDECLS
#endif
