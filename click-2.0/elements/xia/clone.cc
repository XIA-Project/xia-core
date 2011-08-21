#include <click/config.h>
#include "clone.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

Clone::Clone() : _count(0), _next(0)
{
}

Clone::~Clone()
{
    for (size_t i = 0; i < _packets.size(); i++)
        _packets[i]->kill();
    _packets.clear();
}

int
Clone::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int count;
    bool shared_skbs = false;
    if (cp_va_kparse(conf, this, errh,
                   "COUNT", cpkP+cpkM, cpInteger, &count,
                   "SHARED_SKBS", 0, cpBool, &shared_skbs,
                   cpEnd) < 0)
        return -1;
    _count = count;
    _shared_skbs = shared_skbs;
    click_chatter("Packet cloning %d packets", count);
    return 0;
}

void
Clone::push(int /*port*/, Packet *p)
{
    if (_packets.size() == 65536)
        click_chatter("Using more than 65536 packets for cloning; is the code correct?");

    _packets.push_back(p);
}

Packet* Clone::pull(int /*port*/)
{
    if (_packets.size() == 0) return NULL;
    if (_count<=0) return NULL;

    _count--;

    if (_count<=0) click_chatter("No more packet cloning");
    //return _packet->clone()->uniqueify();
    if (++_next >= _packets.size())
        _next = 0;
    if (!_shared_skbs)
        return _packets[_next]->clone();
    else {
#if CLICK_LINUXMODULE
        atomic_inc(&_packets[_next]->skb()->users);
        return _packets[_next];
#else
        // SHARED_SKBS is not effective for userlevel
        return _packets[_next]->clone();
#endif
    }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Clone)
//ELEMENT_MT_SAFE(Clone)
