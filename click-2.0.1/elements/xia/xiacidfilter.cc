#include <click/config.h>
#include "xiacidfilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <iostream>
#include <click/xiatransportheader.hh>

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

void XIACidFilter::handleXtransportPacket(Packet *p)
{
	WritablePacket *pIn = p->uniqueify();
	XIAHeader xiah(pIn->xia_header());
	TransportHeader thdr(pIn);

	if(thdr.pkt_info() != TransportHeader::DATA)
		return;

	WritablePacket *pReply = WritablePacket::make(0, thdr.payload(), xiah.plen() - thdr.hlen(), 0);

	checked_output_push(PORT_OUT_XCACHE, pReply);
}

void XIACidFilter::handleXcachePacket(Packet *p)
{
	std::cout << "CID FILTER Packet received from xcache\n";
}

void XIACidFilter::push(int port, Packet *p)
{
	switch(port) {
	case PORT_IN_XCACHE:
		handleXcachePacket(p);
		break;
	case PORT_IN_XTRANSPORT:
		handleXtransportPacket(p);
		break;
	default:
		std::cout << "Should not happen\n";
	}
	p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIACidFilter)
ELEMENT_MT_SAFE(XIACidFilter)
