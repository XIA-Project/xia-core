/*
 * printstats.{cc,hh} -- prints packet processing stats regularly.
 */

#include <click/config.h>
#include "printstats.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#if CLICK_USERLEVEL
#include <stdlib.h>
#endif
CLICK_DECLS

PrintStats::PrintStats()
{
    _check_packet = 1000000;    // check the interval every 1M packets
    _interval = 1000000;     // 1 sec
    _pps = true;
    _bps = true;

    struct timeval tv;
#if CLICK_USERLEVEL
    gettimeofday(&tv, NULL);
#else
    do_gettimeofday(&tv);
#endif
    _last_time = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;

    _num_packets = 0;
    _num_bits = 0;
}

PrintStats::~PrintStats()
{
}

int
PrintStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_kparse(conf, this, errh,
			"CHECK_PACKET", 0, cpInteger, &_check_packet,
			"INTERVAL", 0, cpInteger, &_interval,
			"PPS", 0, cpBool, &_pps,
			"BPS", 0, cpBool, &_bps,
			cpEnd);
}

Packet *
PrintStats::simple_action(Packet *p)
{
    ++_num_packets;
    _num_bits += p->length() * 8;

#if USERLAND
    if (_num_packets %_check_packet == 0) {
#else
    if (((int32_t)_num_packets) % (int32_t)_check_packet == 0) {
#endif
        //click_chatter(".");
        struct timeval tv;
#if CLICK_USERLEVEL
        gettimeofday(&tv, NULL);
#else
        do_gettimeofday(&tv);
#endif
        uint64_t current_time = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
        uint64_t diff = current_time - _last_time;
        if (diff > static_cast<uint64_t>(_interval)) {
            if (_pps)
                click_chatter("Mpps: %lf\n", static_cast<double>(_num_packets) / (static_cast<double>(diff) / 1000000.) / 1000000.);
            if (_bps)
                click_chatter("Mbps: %lf\n", static_cast<double>(_num_bits) / (static_cast<double>(diff) / 1000000.) / 1000000.);

            _last_time = current_time;
            _num_packets = 0;
            _num_bits = 0;
        }
    }
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintStats)
ELEMENT_MT_SAFE(PrintStats)
