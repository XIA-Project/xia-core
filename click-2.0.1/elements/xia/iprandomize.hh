#ifndef CLICK_IPRANDOMIZE_HH
#define CLICK_IPRANDOMIZE_HH
#include <click/element.hh>
#include "../ip/iproutetable.hh"
#include <click/zipf.h>
CLICK_DECLS

/*
=c
IPRandomize()

=s ip
randomizes an address of an IP packet if the address is 0.0.0.0.

=e

IPRandomize()
*/

class IPRandomize : public Element { public:

    IPRandomize();
    ~IPRandomize();

    const char *class_name() const		{ return "IPRandomize"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    Packet *simple_action(Packet *);

private:
    unsigned short _xsubi[3];
    int _max_cycle;
    int _current_cycle;
    int _offset;
    IPRouteTable *_routeTable;
    Zipf _zipf;
    static uint32_t* _zipf_cache;
    static uint32_t* _ip_cache;
    static bool _ip_cache_initialized;
};

CLICK_ENDDECLS
#endif
