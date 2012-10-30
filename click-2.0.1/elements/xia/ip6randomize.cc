/*
 * ip6randomize.{cc,hh} -- randomizes an address of an IPv6 packet
 */

#include <click/config.h>
#include "ip6randomize.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ip6.h>
#include <click/ip6address.hh>
#if CLICK_USERLEVEL
#include <stdlib.h>
//#elif CLICK_LINUXMODULE
#endif
CLICK_DECLS

IP6Randomize::IP6Randomize()
{
    _xsubi[0] = 1;
    _xsubi[1] = 2;
    _xsubi[2] = 3;

    _current_cycle = 0;
    _max_cycle = 1000000000;
}

IP6Randomize::~IP6Randomize()
{
}

int
IP6Randomize::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_kparse(conf, this, errh,
			"MAX_CYCLE", 0, cpInteger, &_max_cycle,
			cpEnd);
}

Packet *
IP6Randomize::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    if (!p)
        return 0;

    click_ip6 *hdr = p->ip6_header();

    if (hdr->ip6_src.in6_u.u6_addr32[0] == 0 &&
        hdr->ip6_src.in6_u.u6_addr32[1] == 0 &&
        hdr->ip6_src.in6_u.u6_addr32[2] == 0 &&
        hdr->ip6_src.in6_u.u6_addr32[3] == 0) {
#if CLICK_USERLEVEL
        hdr->ip6_src.in6_u.u6_addr32[0] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_src.in6_u.u6_addr32[1] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_src.in6_u.u6_addr32[2] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_src.in6_u.u6_addr32[3] = static_cast<uint32_t>(nrand48(_xsubi));
#elif CLICK_LINUXMODULE
        hdr->ip6_src.in6_u.u6_addr32[0] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[1] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[2] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[3] = static_cast<uint32_t>(random32());
#else 
        XXX  insert your own 
#endif

        if (++_current_cycle == _max_cycle) {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
        }
    }
    if (hdr->ip6_dst.in6_u.u6_addr32[0] == 0 &&
        hdr->ip6_dst.in6_u.u6_addr32[1] == 0 &&
        hdr->ip6_dst.in6_u.u6_addr32[2] == 0 &&
        hdr->ip6_dst.in6_u.u6_addr32[3] == 0) {
#if CLICK_USERLEVEL
        hdr->ip6_dst.in6_u.u6_addr32[0] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_dst.in6_u.u6_addr32[1] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_dst.in6_u.u6_addr32[2] = static_cast<uint32_t>(nrand48(_xsubi));
        hdr->ip6_dst.in6_u.u6_addr32[3] = static_cast<uint32_t>(nrand48(_xsubi));
#elif CLICK_LINUXMODULE
        hdr->ip6_src.in6_u.u6_addr32[0] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[1] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[2] = static_cast<uint32_t>(random32());
        hdr->ip6_src.in6_u.u6_addr32[3] = static_cast<uint32_t>(random32());
#else 
        XXX  insert your own 
#endif

        if (++_current_cycle == _max_cycle) {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
        }

		// route table lookup relies on this
        SET_DST_IP6_ANNO(p, hdr->ip6_dst);
    }

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IP6Randomize)
ELEMENT_MT_SAFE(IP6Randomize)
