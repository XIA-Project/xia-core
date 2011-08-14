#ifndef CLICK_IP6RANDOMIZE_HH
#define CLICK_IP6RANDOMIZE_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
IP6Randomize()

=s ip
randomizes an address of an IPv6 packet if the address is 0::0.

=e

IP6Randomize()
*/

class IP6Randomize : public Element { public:

    IP6Randomize();
    ~IP6Randomize();

    const char *class_name() const		{ return "IP6Randomize"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    Packet *simple_action(Packet *);

private:
    unsigned short _xsubi[3];
    int _max_cycle;
    int _current_cycle;
};

CLICK_ENDDECLS
#endif
