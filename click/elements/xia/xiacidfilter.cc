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

	std::cout << "CID FILTER Packet Recvd from transport\n";

// FIXME: IS IT SAFE TO ASSUME IF NO FLAGS ARE SET IT"S JUST DATA
// THIS TRANSPORT MAY ROLL ACKs UP WITH DATA?
//	if (thdr.pkt_info() != TransportHeader::DATA)
//	if (thdr.flags() != 0)
//		return;

	checked_output_push(PORT_OUT_XCACHE, pIn);
	std::cout << "CID FILTER sent to xcache\n";
}

void XIACidFilter::handleXcachePacket(Packet * /* p */)
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
