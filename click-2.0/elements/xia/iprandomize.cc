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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
CLICK_DECLS

IPRandomize::IPRandomize() :_routeTable(0), _zipf(1.2) 
{
    _xsubi[0] = 1;
    _xsubi[1] = 2;
    _xsubi[2] = 3;

    _current_cycle = 0;
    _max_cycle = 1000000000;
}

IPRandomize::~IPRandomize()
{
    if (_zipf_cache) {
        delete[] _zipf_cache;
		_zipf_cache = NULL;
		if (_ip_cache) {
			delete[] _ip_cache;
			_ip_cache = NULL;
		}
    }
}

uint32_t * IPRandomize::_zipf_cache = NULL;
uint32_t * IPRandomize::_ip_cache = NULL;
bool IPRandomize::_ip_cache_initialized = false;

int
IPRandomize::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element* routing_table_elem = NULL;
    int ret = cp_va_kparse(conf, this, errh,
			"ROUTETABLENAME", 0, cpElement, &routing_table_elem,
			"MAX_CYCLE", 0, cpInteger, &_max_cycle,
			"OFFSET", 0, cpInteger, &_offset,
			cpEnd);

    if (ret<0)
		return ret;

#if CLICK_USERLEVEL
    _routeTable = dynamic_cast<IPRouteTable*>(routing_table_elem);
    click_chatter("route table %x", _routeTable); 
#else
    _routeTable = reinterpret_cast<IPRouteTable*>(routing_table_elem);
#endif
    for (int i = 0; i < _offset; i++)
	    nrand48(_xsubi);

    _current_cycle = _offset;

    _zipf = Zipf(1.2, _max_cycle-1);

    if (_zipf_cache == NULL) {
		_zipf_cache = new uint32_t[_max_cycle*100];
		//_zipf_arbit = Zipf(1.2, 1000000000);
		for (int i = 0;i < _max_cycle * 100; i++) {
			uint32_t v;
			do {
				v = _zipf.next();
			} while (v >= (uint32_t)_max_cycle);
			_zipf_cache[i] = v;
		}
        _ip_cache = new uint32_t[_max_cycle];
    }
    srand(191287);

    return 0;
}

Packet *
IPRandomize::simple_action(Packet *p_in)
{
    //unsigned short xsubi_next[3];
    WritablePacket *p = p_in->uniqueify();
    if (!p)
        return 0;

    click_ip *hdr = p->ip_header();
    assert(_zipf_cache);
    //uint32_t seed  = _zipf_cache[_current_cycle]; /* zipf */
    if (!_ip_cache_initialized && (click_current_thread_id == 1)) {
		if (_routeTable) { 
			IPAddress gw;
			int port;
			uint32_t ip_dst;
			Vector<uint32_t> prefix;
			for (int i = 0; i < 1 << 24; i++) {
				uint32_t ip = i<<8;
				ip_dst = htonl(ip);
				port = _routeTable->lookup_route(IPAddress(ip_dst), gw);
				if (port > 0)
					prefix.push_back(ip_dst);
			}
			for (int i = 0; i < _max_cycle; i++) {
				int index = rand() % prefix.size();
				uint32_t ip = prefix[index]; // + htonl(rand()%256);
				_ip_cache[i] = ip;
				if (i < 100) {
					struct in_addr addr;
					addr.s_addr = ip;
					click_chatter("IP addr %i %s ", i, inet_ntoa(addr));
				}
			}
		}
		_ip_cache_initialized = true;
    } else if (!_ip_cache_initialized) {
		p->kill();
		return NULL;
    }

    if (hdr->ip_src.s_addr == 0) {
#if CLICK_USERLEVEL
        hdr->ip_src.s_addr = static_cast<uint32_t>(nrand48(_xsubi));
#elif CLICK_LINUXMODULE
        hdr->ip_src.s_addr = static_cast<uint32_t>(random32());
#else
	XXX
#endif

        if (++_current_cycle == _max_cycle) {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
			for (int i = 0; i < _offset; i++)
				nrand48(_xsubi);
			_current_cycle = _offset;
        }
    }
    if (hdr->ip_dst.s_addr == 0) {
#if CLICK_USERLEVEL
		//assert(seed<_max_cycle);
		//memcpy(&xsubi_next[1], &seed, 2);
		//memcpy(&xsubi_next[2], reinterpret_cast<char *>(&seed)+2 , 2);
		//xsubi_next[0]= xsubi_next[2]+ xsubi_next[1];
		//hdr->ip_dst.s_addr = static_cast<uint32_t>(nrand48(xsubi_next));
        hdr->ip_dst.s_addr =  _ip_cache[_zipf_cache[_current_cycle]];
#elif CLICK_LINUXMODULE
        uint32_t rand = static_cast<uint32_t>(random32());
        if (rand >> 24 >= 224) {
			char* msb = (char *)(&rand); 
			*msb = (char)(rand % 223 + 1);
        }
        hdr->ip_dst.s_addr = rand;
#else
		XXX
#endif

        if (++_current_cycle == _max_cycle) {
            _xsubi[0] = 1;
            _xsubi[1] = 2;
            _xsubi[2] = 3;
            _current_cycle = 0;
			for (int i = 0; i < _offset; i++)
			  nrand48(_xsubi);
			_current_cycle = _offset;
        }

		// route table lookup relies on this
        p->set_dst_ip_anno(IPAddress(hdr->ip_dst));
    }

    // TODO: may need to update checksum

	/*
    if (_routeTable) { 
	    IPAddress gw;
	    int port = _routeTable->lookup_route(IPAddress(hdr->ip_dst), gw);
	    if (port<0)  {
		    p->kill();
		    return NULL;
	    }
    }
	*/
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPRandomize)
ELEMENT_MT_SAFE(IPRandomize)
