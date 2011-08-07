#include <click/config.h>
#include "clone.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

Clone::Clone() :_packet(0), _count(0), _first_replacement(true)
{
}

Clone::~Clone()
{
    if (_packet)
        _packet->kill();
}

int
Clone::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int count;
    if (cp_va_kparse(conf, this, errh,
                   "COUNT", cpkP+cpkM, cpInteger, &count,
                   cpEnd) < 0)
        return -1;
    _count = count;
    click_chatter("Packet cloning %d packets", count);
    return 0;
}

void
Clone::push(int /*port*/, Packet *p)
{
    if (_packet) {
        if (_first_replacement) {
            click_chatter("Replacing the template packet for cloning");
            _first_replacement = false;
        }
        _packet->kill();
    }

    _packet = p;
}

Packet* Clone::pull(int /*port*/)
{
    if (_packet==NULL) return NULL;
    if (_count<=0) return NULL;

    _count--;

    if (_count<=0) click_chatter("No more packet cloning");
    //return _packet->clone()->uniqueify();
    return _packet->clone();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Clone)
ELEMENT_MT_SAFE(Clone)
