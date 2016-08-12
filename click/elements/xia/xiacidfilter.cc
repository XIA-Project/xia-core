#include <click/config.h>
#include "xiacidfilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <iostream>
#include <click/xiatransportheader.hh>
#include "xlog.hh"

CLICK_DECLS

XIACidFilter::XIACidFilter()
{
}

XIACidFilter::~XIACidFilter()
{
}

int XIACidFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
	bool _enable;

	if (cp_va_kparse(conf, this, errh,
					"ENABLE", cpkP + cpkM, cpBool, &_enable,
					cpEnd) < 0) {

		return -1;
	}

	enabled = _enable;
	return 0;
}

void XIACidFilter::handleNetworkPacket(Packet *p)
{
	WritablePacket *pIn = p->uniqueify();

	DBG("CID FILTER sending content packet to xcache\n");
	checked_output_push(PORT_OUT_XCACHE, pIn);
}

void XIACidFilter::handleXcachePacket(Packet * /* p */)
{
	//INFO("CID FILTER Packet received from xcache\n");
}

void XIACidFilter::push(int port, Packet *p)
{
	if (enabled) {
		switch(port) {
		case PORT_IN_XCACHE:
			handleXcachePacket(p);
			break;
		case PORT_IN_NETWORK:
			handleNetworkPacket(p);
			break;
		default:
			ERROR("Should not happen\n");
		}
	}
	p->kill();
}

int XIACidFilter::toggle(const String &conf, Element *e, void * /*vparam*/, ErrorHandler *errh)
{
	bool _enable;
	XIACidFilter *f = static_cast<XIACidFilter *>(e);

	if (cp_va_kparse(conf, f, errh,
					"ENABLE", cpkP + cpkM, cpBool, &_enable,
					cpEnd) < 0) {

		return -1;
	}

	f->enabled = _enable;
	return 0;
}

String XIACidFilter::status(Element *e, void * /*thunk*/)
{
	XIACidFilter* f = static_cast<XIACidFilter*>(e);

	return f->enabled ? "enabled\n" : "disabled\n";
}

void XIACidFilter::add_handlers()
{
	add_read_handler("status", status, 0);
	add_write_handler("enable", toggle, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACidFilter)
ELEMENT_MT_SAFE(XIACidFilter)
