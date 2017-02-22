#ifndef CLICK_CHECKXIAHEADER_HH
#define CLICK_CHECKXIAHEADER_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * CheckXIAHeader([OFFSET])
 *
 * =s ip
 * basic sanity test for XIP packets
 *
 * =d
 *
 * Ensures that the actual packet length matchs what the xip header reports.
 * Discards any packets that fail the tests.
 *
 * The XIA header starts OFFSET bytes into the packet. Default OFFSET is 0.
 *
 * =a CheckIPHeader */

class CheckXIAHeader : public Element {

  public:
	CheckXIAHeader()  {};
	~CheckXIAHeader() {};

	const char *class_name() const { return "CheckXIAHeader"; }
	const char *port_count() const { return PORTS_1_1; }
	int configure(Vector<String> &, ErrorHandler *);

	Packet *simple_action(Packet *);
	bool is_valid_xia_header(Packet *p);

  private:
	int _offset;
};

CLICK_ENDDECLS
#endif
