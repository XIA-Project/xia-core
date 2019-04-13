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

void
XIAOverlayFilter::push(int, Packet *p)
{
	// Does this packet come with an IP address and port annotated?
	if(!(p->dst_ip_anno().empty()) && (DST_PORT_ANNO(p) != 0)) {
		printf("XIAOverlayFilter: found overlay packet\n");
		printf("XIAOverlayFilter: going out port %d\n", XIA_PAINT_ANNO(p));
		output(0).push(p);
	} else {
		// TODO: add check to make sure the XIA PAINT ANNO is there
		output(1).push(p);
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAOverlayFilter)
ELEMENT_MT_SAFE(XIAOverlayFilter)
