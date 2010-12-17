#ifndef CLICK_DECXIAHLIM_HH
#define CLICK_DECXIAHLIM_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * DecXIAHLIM([keyword])
 * =s ip
 * decrements XIA hop limit, drops dead packets
 * =d
 * Expects XIA packet as input.
 * If the hlim is <= 1 (i.e. has expired),
 * DecXIAHLIM sends the packet to output 1 (or discards it if there is no
 * output 1).
 * Otherwise it decrements the hlim,
 * and sends the packet to output 0.
 *
 * Ordinarily output 1 is connected to an XCMP error packet generator.
 *
 * =over 8
 *
 * =item ACTIVE
 *
 * Boolean.  If false, do not decrement any packets' HLIMs.  Defaults to true.
 *
 * =back
 *
 * =a DecIPTTL 
 */

class DecXIAHLIM : public Element { public:

    DecXIAHLIM();
    ~DecXIAHLIM();

    const char *class_name() const		{ return "DecXIAHLIM"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    atomic_uint32_t _drops;
    bool _active;

};

CLICK_ENDDECLS
#endif
