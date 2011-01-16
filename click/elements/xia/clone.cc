#include <click/config.h>
#include "clone.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

Clone::Clone()
{
}

Clone::~Clone()
{
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
