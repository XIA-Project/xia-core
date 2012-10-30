/*
 * xiarandomize.{cc,hh} -- randomizes an address of an XIA packet
 */

#include <click/config.h>
#include "xiarandomize.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/xia.h>
#if CLICK_USERLEVEL
#include <stdlib.h>
#endif
CLICK_DECLS
#define PURE_RANDOM

XIARandomize::XIARandomize() : _zipf(1.3)
{
    assert(CLICK_XIA_XID_ID_LEN % sizeof(uint32_t) == 0);

#if CLICK_USERLEVEL
    _xsubi_det[0] = 1;
    _xsubi_det[1] = 2;
    _xsubi_det[2] = 3;
#elif CLICK_LINUXMODULE
    prandom32_seed(&_deterministic, 1239);
#endif

    _current_cycle = 0;
    _max_cycle = 1000000000;

#if CLICK_USERLEVEL
#if HAVE_MULTITHREAD
    _xsubi_arb[0] = 7 * click_current_thread_id + 1;
    _xsubi_arb[1] = 5 * click_current_thread_id - 1;
    _xsubi_arb[2] = 3 * click_current_thread_id + 1;
#else
    _xsubi_arb[0] = 0;
    _xsubi_arb[1] = 1;
    _xsubi_arb[2] = 2;
#endif
#elif CLICK_LINUXMODULE
    prandom32_seed(&_arbitrary, 123999);
#endif
}

uint32_t * XIARandomize::_zipf_cache = NULL;
atomic_uint32_t XIARandomize::_zipf_cache_lock;

XIARandomize::~XIARandomize()
{
    if (_zipf_cache) {
        delete[] _zipf_cache;
		_zipf_cache = NULL;
    }
}

int
XIARandomize::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int offset=0;
    int multiplier =0; 

    if (cp_va_kparse(conf, this, errh,
			"XID_TYPE", cpkP+cpkM, cpXIDType, &_xid_type,
			"MAX_CYCLE", 0, cpInteger, &_max_cycle,
			"OFFSET", 0, cpInteger, &offset,
			"MULTIPLIER", 0, cpInteger, &multiplier,
			cpEnd)<0) 
		return -1;
    _offset = offset *multiplier;

    for (int i = 0; i < _offset; i++) {
        nrand48(_xsubi_det);
        nrand48(_xsubi_arb);
    }
    _current_cycle = _offset;
    _zipf = Zipf(1.2, _max_cycle-1);

    if (_zipf_cache_lock.swap(1) == 0) {
        if (!_zipf_cache) {
            click_chatter("generating zipf cache of size %d\n", _max_cycle*100);
            _zipf_cache = new uint32_t[_max_cycle*100];
            //_zipf_arbit = Zipf(1.2, 1000000000);
            for (int i = 0; i < _max_cycle * 100; i++) {
				uint32_t v;
				do {
					v = _zipf.next();
				} while (v >= (uint32_t)_max_cycle);
                _zipf_cache[i] = v;
            }
        }
        else
            click_chatter("zipf cache seems already created\n");

        asm volatile ("" : : : "memory");
        _zipf_cache_lock = 0;
    }
    else {
        if (_zipf_cache_lock.value() == 1) {
            click_chatter("waiting for the other thread to create zipf cache\n");
            while (_zipf_cache_lock.value() == 1)
                ;
        }
        else
            click_chatter("zipf cache seems already created\n");
    }

    /*
    for (int i = 0; i < _max_cycle * 100; i++) {
            assert(_zipf_cache[i]< _max_cycle);
    }
    */
    return 0;
}

Packet *
XIARandomize::simple_action(Packet *p_in)
{
    unsigned short xsubi_next[3];
    WritablePacket *p = p_in->uniqueify();
    if (!p)
        return 0;

    click_xia *hdr = p->xia_header();

    assert(_zipf_cache);
    uint32_t seed = _zipf_cache[_current_cycle]; // zipf
    //uint32_t seed = rand() % _max_cycle;    // uniform

    for (size_t i = 0; i < hdr->dnode + hdr->snode; i++)
    {
        struct click_xia_xid_node& node = hdr->node[i];
        if (node.xid.type == 1) {
			// Determinstic Random
            node.xid.type = _xid_type;
            uint8_t* xid = node.xid.id;
            const uint8_t* xid_end = xid + CLICK_XIA_XID_ID_LEN;
#ifdef PURE_RANDOM
			//uint32_t seed  = static_cast<uint32_t>(nrand48(_xsubi_det)) % _max_cycle; /* uniform */
			assert(seed < (uint32_t)_max_cycle);
			memcpy(&xsubi_next[1], &seed, 2);
			memcpy(&xsubi_next[2], reinterpret_cast<char *>(&seed)+2 , 2);
			xsubi_next[0]= xsubi_next[2]+ xsubi_next[1];
#endif

            while (xid != xid_end) {
#if CLICK_USERLEVEL
#ifdef PURE_RANDOM
				*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi_next));
#else
				*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(_xsubi_det));
#endif
#elif CLICK_LINUXMODULE
				*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(prandom32(&_deterministic));
#endif
                xid += sizeof(uint32_t);
            }

            if (++_current_cycle == _max_cycle*100) {
				_current_cycle = 0;
#if CLICK_USERLEVEL
				_xsubi_det[0] = 1;
				_xsubi_det[1] = 2;
				_xsubi_det[2] = 3;

				for (int i = 0; i < _offset; i++)
					nrand48(_xsubi_det);
				_current_cycle = _offset;
#elif CLICK_LINUXMODULE
				prandom32_seed(&_deterministic, 0);
#endif
			}
        }
        else if (node.xid.type == 2)
        {
			// Abitrary random
            node.xid.type = _xid_type;
            uint8_t* xid = node.xid.id;
            const uint8_t* xid_end = xid + CLICK_XIA_XID_ID_LEN;
#ifdef PURE_RANDOM
			seed  +=  (i+1)* 13 + _max_cycle;
			memcpy(&xsubi_next[1], &seed, 2);
			memcpy(&xsubi_next[2], &(reinterpret_cast<char *>(&seed)[2]), 2);
			xsubi_next[0]= xsubi_next[2]+ xsubi_next[1];
#endif

            while (xid != xid_end) {
#if CLICK_USERLEVEL
#ifdef PURE_RANDOM
				*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi_next));
#else
				*reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(_xsubi_arb));
#endif
#elif CLICK_LINUXMODULE
                *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(prandom32(&_arbitrary));
#endif
                xid += sizeof(uint32_t);
            }
        }
    }

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIARandomize)
ELEMENT_MT_SAFE(XIARandomize)
