#ifndef CLICK_XIAIPENCAP_HH
#define CLICK_XIAIPENCAP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/ip.h>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <clicknet/udp.h>
CLICK_DECLS

/*
 * =c
 * XIAIPEncap()
 *
 * =s ip
 * Encapsulates an XIA packet into an IP Packet
 *
 * =d
 *
 * =n
 *
 * =a IPEncap */

#define IP_XID_UDP_PORT 1001

class XIAIPEncap : public Element { public:

    XIAIPEncap();
    ~XIAIPEncap();
  
    const char *class_name() const		{ return "XIAIPEncap"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
    const char *flags() const             { return "A"; }
  
    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return true; }
    void add_handlers();
  
    Packet *simple_action(Packet *);
  
  private:
    struct in_addr _saddr;
    struct in_addr _daddr;
    uint16_t _sport;
    uint16_t _dport;
    bool _cksum;
    bool _use_dst_anno;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    bool _aligned;
    bool _checked_aligned;
#endif
    atomic_uint32_t _id;

    static String read_handler(Element *, void *);
};

CLICK_ENDDECLS
#endif
