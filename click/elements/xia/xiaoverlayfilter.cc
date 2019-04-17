#include <click/config.h>
#include "xiaoverlayfilter.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAOverlayFilter::XIAOverlayFilter()
{
}

XIAOverlayFilter::~XIAOverlayFilter()
{
}

/*
int
XIAOverlayFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = XIA_PAINT_ANNO_OFFSET;
    if (Args(conf, this, errh).read_p("ANNO", AnnoArg(1), anno).complete() < 0)
	  return -1;
    _anno = anno;
    return 0;
}
*/

/* Filter Overlay packets
 *  input[0] = Incoming packet from network stack
 *  input[1] = Incoming packet from network
 *  output[0] = Outgoing overlay packet
 *  output[1] = Outgoing XIA packet
 *  output[2] = Incoming overlay packet from network
 *  output[3] = Incoming invalid overlay packet from network
 */
void
XIAOverlayFilter::push(int port, Packet *p)
{
	if(port == 0) {
		// Does this packet come with an IP address and port annotated?
		if(!(p->dst_ip_anno().empty()) && (DST_PORT_ANNO(p) != 0)) {
			printf("XIAOverlayFilter: found overlay packet\n");
			printf("XIAOverlayFilter: going out port %d\n", XIA_PAINT_ANNO(p));
			output(0).push(p);
		} else {
			// TODO: add check to make sure the XIA PAINT ANNO is there
			output(1).push(p);
		}
	} else if(port == 1) {
		printf("XIAOverlayFilter: processing incoming pkt from network\n");
		if(p->src_ip_anno().empty()) {
			printf("XIAOverlayPacket: src IP annotation was empty\n");
		} else {
			printf("XIAOverlayPacket: src IP annotation %s\n",
					IPAddress(p->src_ip_anno()).unparse().c_str());
		}
		if( !(p->src_ip_anno().empty()) ) {
			// Overlay XIA packet. XIAOverlaySocket puts SRC_IP_ANNO on it
			printf("XIAOverlayFilter: rcvd overlay pkt from net\n");
			output(2).push(p);
		} else {
			// Regular XIA packet with ethernet header
			output(3).push(p);
		}
	} else {
		printf("XIAOverlayFilter: ERROR invalid incoming port\n");
		p->kill();
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAOverlayFilter)
ELEMENT_MT_SAFE(XIAOverlayFilter)
