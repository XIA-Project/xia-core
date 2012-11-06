/*
 * xiadechlim.{cc,hh} -- element decrements XIA packet's hop limit
 */

#include <click/config.h>
#include "xiadechlim.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <clicknet/xia.h>
CLICK_DECLS

XIADecHLIM::XIADecHLIM()
    : _active(true)
{
	_drops = 0;
}

XIADecHLIM::~XIADecHLIM()
{
}

int
XIADecHLIM::configure(Vector<String> &conf, ErrorHandler *errh)
{
	return cp_va_kparse(conf, this, errh,
			"ACTIVE", 0, cpBool, &_active,
			cpEnd);
}

Packet *
XIADecHLIM::simple_action(Packet *p)
{
	assert(p->has_network_header());

	if (!_active)
		return p;

	const click_xia *hdr = p->xia_header();

	if (hdr->hlim <= 1) {
    	++_drops;
		SET_XIA_PAINT_ANNO(p, UNREACHABLE);
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
XIADecHLIM::add_handlers()
{
	add_data_handlers("drops", Handler::OP_READ, &_drops);
	add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX, &_active);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIADecHLIM)
ELEMENT_MT_SAFE(XIADecHLIM)
