#ifndef CLICK_MARKXIAHEADER_HH
#define CLICK_MARKXIAHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * MarkXIAHeader([OFFSET])
 * =s ip
 * sets XIA header annotation
 * =d
 *
 * Marks packets as XIA packets by setting the XIA Header annotation. The XIA 
 * header starts OFFSET bytes into the packet. Default OFFSET is 0.
 *
 * Does not check length fields for sanity or shorten packets to the XIA length.
 *
 * =a MarkIPHeader */

class MarkXIAHeader : public Element {

  int _offset;

 public:

  MarkXIAHeader();
  ~MarkXIAHeader();

  const char *class_name() const		{ return "MarkXIAHeader"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
