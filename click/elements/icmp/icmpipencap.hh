#ifndef CLICK_ICMPIPENCAP_HH
#define CLICK_ICMPIPENCAP_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

class ICMPIPEncap : public Element { public:

    ICMPIPEncap();
    ~ICMPIPEncap();

    const char *class_name() const		{ return "ICMPIPEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
    const char *flags() const			{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();

    Packet *simple_action(Packet *);

  private:

    struct in_addr _src;
    struct in_addr _dst;
    uint16_t _icmp_id;
    uint16_t _ip_id;
    uint8_t _icmp_type;
    uint8_t _icmp_code;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
#endif

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
