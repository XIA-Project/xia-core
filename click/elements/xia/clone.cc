#include <click/config.h>
#include "clone.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

Clone::Clone() :_packet(0)
{
}

Clone::~Clone()
{
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
    return 0;
}

void
Clone::push(int /*port*/, Packet *p)
{
    if (_packet==NULL)
        _packet = p;
    return;
}

Packet* Clone::pull(int /*port*/)
{
    if (_packet==NULL) return NULL;
    return _packet->clone();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Clone)
ELEMENT_MT_SAFE(Clone)
