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
	/* Constructor */
}

XIACidFilter::~XIACidFilter()
{
	/* Destructor */
}

int XIACidFilter::configure(Vector<String> &confStr, ErrorHandler *errh)
{
	(void)confStr;
	(void)errh;

	return 0;

	// FIXME: Do we need to configure something?
}

void XIACidFilter::handleNetworkPacket(Packet *p)
{
	WritablePacket *pIn = p->uniqueify();

	INFO("CID FILTER sending content packet to xcache\n");
	checked_output_push(PORT_OUT_XCACHE, pIn);
}

void XIACidFilter::handleXcachePacket(Packet * /* p */)
{
	//INFO("CID FILTER Packet received from xcache\n");
}

void XIACidFilter::push(int port, Packet *p)
{
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
	p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACidFilter)
ELEMENT_MT_SAFE(XIACidFilter)
