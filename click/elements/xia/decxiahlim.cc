/*
 * decxiahlim.{cc,hh} -- element decrements XIA packet's hop limit
 */

#include <click/config.h>
#include "decxiahlim.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
CLICK_DECLS

DecXIAHLIM::DecXIAHLIM()
    : _active(true)
{
    _drops = 0;
}

DecXIAHLIM::~DecXIAHLIM()
{
}

int
DecXIAHLIM::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return cp_va_kparse(conf, this, errh,
			"ACTIVE", 0, cpBool, &_active,
			cpEnd);
}

Packet *
DecXIAHLIM::simple_action(Packet *p)
{
    assert(p->has_network_header());
    if (!_active)
	return p;
    const click_xia *hdr = p->xia_header();

    if (hdr->hlim <= 1) {
	++_drops;
	checked_output_push(1, p);
	return 0;
    } else {
	WritablePacket *q = p->uniqueify();
	if (!q)
	    return 0;
	click_xia *hdr = q->xia_header();
	--hdr->hlim;

	return q;
    }
}

void
DecXIAHLIM::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX, &_active);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DecXIAHLIM)
ELEMENT_MT_SAFE(DecXIAHLIM)
