#include <click/config.h>
#include "clone.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

Clone::Clone() : _count(0), _next(0), _wait_until(0), _active(true)
{
}

Clone::~Clone()
{
    for (int i = 0; i < _packets.size(); i++)
        _packets[i]->kill();
    _packets.clear();
}

int
Clone::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int count;
    int waituntil;
    bool shared_skbs = false;
    if (cp_va_kparse(conf, this, errh,
                   "COUNT", cpkP+cpkM, cpInteger, &count,
                   "SHARED_SKBS", 0, cpBool, &shared_skbs,
                   "WAITUNTIL", 0, cpInteger, &waituntil,
                   "ACTIVE", 0, cpBool, &_active,
                   cpEnd) < 0)
        return -1;

    _count = count;
    _shared_skbs = shared_skbs;

    if (waituntil > 0)
		_wait_until = waituntil;
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
    if (_packets.size() == 0 || _packets.size() < _wait_until)
		return NULL;
    if (_count <= 0)
		return NULL;
    if (!_active)
		return NULL;

    _count--;

    if (_count <= 0)
		click_chatter("No more packet cloning");
		//return _packet->clone()->uniqueify();
    if (++_next >= _packets.size())
        _next = 0;

    if (!_shared_skbs)
        return _packets[_next]->clone();
    else {
#if CLICK_LINUXMODULE
        __builtin_prefetch(_packets[_next]->skb()->data + 0, 0, 0);
        __builtin_prefetch(_packets[_next]->skb()->data + 64, 0, 0);
        //atomic_inc(&_packets[_next]->skb()->users);
        (*(int*)&_packets[_next]->skb()->users)++;
        return _packets[_next];
#else
        // SHARED_SKBS is not effective for userlevel
        return _packets[_next]->clone();
#endif
    }
}

void
Clone::add_handlers()
{
    add_write_handler("active", set_handler, 0);
}

int
Clone::set_handler(const String &conf, Element *e, void * /*thunk*/, ErrorHandler * /*errh*/)
{
    Clone* table = static_cast<Clone*>(e);

    if (conf == "true")
		table->_active = true;

    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Clone)
//ELEMENT_MT_SAFE(Clone)
