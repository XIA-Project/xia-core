#ifndef CLICK_PRINTSTATS_HH
#define CLICK_PRINTSTATS_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
PrintStats()

=s ip
prints packet processing stats regularly.

=e

*/

class PrintStats : public Element { public:

    PrintStats();
    ~PrintStats();

    const char *class_name() const		{ return "PrintStats"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    Packet *simple_action(Packet *);

  private:
    int _check_packet;
    int _interval;
    bool _pps;
    bool _bps;

    uint64_t _last_time;
    uint64_t _num_packets;
    uint64_t _num_bits;
};

CLICK_ENDDECLS
#endif
