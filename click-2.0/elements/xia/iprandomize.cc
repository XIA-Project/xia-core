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

IPRandomize::IPRandomize() :_routeTable(0)
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
    Element* routing_table_elem;
    int ret = cp_va_kparse(conf, this, errh,
			"ROUTETABLENAME", 0, cpElement, &routing_table_elem,
			"MAX_CYCLE", 0, cpInteger, &_max_cycle,
			"OFFSET", 0, cpInteger, &_offset,
			cpEnd);
    if (ret<0) return ret;
#if CLICK_USERLEVEL
    _routeTable = dynamic_cast<IPRouteTable*>(routing_table_elem);
    click_chatter("route table %x", _routeTable);
#else
    _routeTable = reinterpret_cast<IPRouteTable*>(routing_table_elem);
#endif
    for (int i=0;i<_offset;i++) {
	    nrand48(_xsubi);
    }
    _current_cycle = _offset;

    return 0;
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
	    for (int i=0;i<_offset;i++) {
	      nrand48(_xsubi);
	    }
	    _current_cycle = _offset;
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
	    for (int i=0;i<_offset;i++) {
	      nrand48(_xsubi);
	    }
	    _current_cycle = _offset;
        }

        p->set_dst_ip_anno(IPAddress(hdr->ip_dst));  // route table lookup relies on this
    }

    // TODO: need to update checksum

    if (_routeTable) { 
	    IPAddress gw;
	    int port = _routeTable->lookup_route(IPAddress(hdr->ip_dst), gw);
	    if (port<0)  {
		    p->kill();
		    return NULL;
	    }
    }
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPRandomize)
ELEMENT_MT_SAFE(IPRandomize)
