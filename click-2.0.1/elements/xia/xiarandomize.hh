#ifndef CLICK_XIARANDOMIZE_HH
#define CLICK_XIARANDOMIZE_HH
#include <click/element.hh>
#include <click/zipf.h>
#include <click/atomic.hh>
CLICK_DECLS

/*
=c
XIARandomize()

=s ip
randomizes an address of an XIA packet if the XID type is 1 (for deterministic random) or 2 (for arbitrary random).

=e

XIARandomize()
*/

class XIARandomize : public Element { public:

    XIARandomize();
    ~XIARandomize();

    const char *class_name() const		{ return "XIARandomize"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    Packet *simple_action(Packet *);

private:
#if CLICK_LINUXMODULE
    struct rnd_state _deterministic;
#else
    unsigned short _xsubi_det[3];
#endif
    int _max_cycle;
    int _current_cycle;

#if CLICK_LINUXMODULE
    struct rnd_state _arbitrary;
#else
    unsigned short _xsubi_arb[3];
#endif

    int _xid_type;
    int _offset;
    Zipf _zipf;
    static uint32_t* _zipf_cache;
    static atomic_uint32_t _zipf_cache_lock;
};

CLICK_ENDDECLS
#endif
