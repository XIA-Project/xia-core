#ifndef CLICK_XIAPAINT_HH
#define CLICK_XIAPAINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

XIAPaint(COLOR [, ANNO])

=s paint

sets packet xiapaint annotations

=d

Sets each packet's xiapaint annotation to COLOR, an integer 0..255.

Paint sets the packet's PAINT annotation by default, but the ANNO argument can
specify any two-byte annotation.

=h color read/write

Get/set the color to paint.

=a PaintTee */

class XIAPaint : public Element { public:

    XIAPaint();
    ~XIAPaint();

    const char *class_name() const		{ return "XIAPaint"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const {return false;}
  //bool can_live_reconfigure() const		{ return true; }
  //void add_handlers();

    Packet *simple_action(Packet *);

  private:

    uint8_t _anno;
    int16_t _color;

};

CLICK_ENDDECLS
#endif
