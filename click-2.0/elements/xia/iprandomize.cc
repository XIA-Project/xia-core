/*
 * iprandomize.{cc,hh} -- randomizes an address of an IP packet
 */

#include <click/config.h>
#include "iprandomize.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#if CLICK_USERLEVEL
#include <stdlib.h>
#elif CLICK_LINUXMODULE
#else
 XXX
#endif
CLICK_DECLS

IPRandomize::IPRandomize()
{
    _xsubi[0] = 1;
    _xsubi[1] = 2;
    _xsubi[2] = 3;

    _current_cycle = 0;
    _max_cycle = 1000000000;
}

IPRandomize::~IPRandomize()
{
}

int
IPRandomize::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_kparse(conf, this, errh,
			"MAX_CYCLE", 0, cpInteger, &_max_cycle,
			cpEnd);
}

Packet *
IPRandomize::simple_action(Packet *p_in)
{
    WritablePacket *p = p_in->uniqueify();
    if (!p)
        return 0;

    click_ip *hdr = p->ip_header();

    if (hdr->ip_src.s_addr == 0)
    {
#if CLICK_USERLEVEL
        hdr->ip_src.s_addr = static_cast<uint32_t>(nrand48(_xsubi));
#elif CLICK_LINUXMODULE
        hdr->ip_src.s_addr = static_cast<uint32_t>(random32());
#else
	XXX
#endif

        if (++_current_cycle == _max_cycle)
        {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
        }
    }
    if (hdr->ip_dst.s_addr == 0)
    {
#if CLICK_USERLEVEL
        hdr->ip_dst.s_addr = static_cast<uint32_t>(nrand48(_xsubi));
#elif CLICK_LINUXMODULE
        uint32_t rand = static_cast<uint32_t>(random32());
        if (rand>>24 >=224) {
	   char* msb =(char*)(&rand); 
	   *msb= (char)(rand % 223+1);
        }
        hdr->ip_dst.s_addr = rand;
#else
	XXX
#endif

        if (++_current_cycle == _max_cycle)
        {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
        }

        p->set_dst_ip_anno(IPAddress(hdr->ip_dst));  // route table lookup relies on this
    }

    // TODO: need to update checksum

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPRandomize)
ELEMENT_MT_SAFE(IPRandomize)
