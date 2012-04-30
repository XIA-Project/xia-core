/*
 * xiaipencap.{cc,hh} -- encapsulates an XIA packet with an IP header
 */

#include <click/config.h>
#include "xiaipencap.hh"
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS

XIAIPEncap::XIAIPEncap()
    : _cksum(true), _use_dst_anno(false)
{
    _id = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    _checked_aligned = false;
#endif
}

XIAIPEncap::~XIAIPEncap()
{
}

int
XIAIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPAddress saddr;
    uint16_t sport = IP_XID_UDP_PORT, dport = IP_XID_UDP_PORT;
    bool cksum;
    String daddr_str;

    if (Args(conf, this, errh)
			.read_mp("SRC", saddr)
			.read("SPORT", IPPortArg(IP_PROTO_UDP), sport)
			.read("DST", AnyArg(), daddr_str)
			.read("DPORT", IPPortArg(IP_PROTO_UDP), dport)
			.read_p("CHECKSUM", BoolArg(), cksum)
			.complete() < 0)
		return -1;

    _use_dst_anno = false;

    _saddr = saddr;
    _sport = htons(sport);
    _dport = htons(dport);

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    if (!_checked_aligned) {
		int ans, c, o;
		ans = AlignmentInfo::query(this, 0, c, o);
		_aligned = (ans && c == 4 && o == 0);
		if (!_aligned)
			errh->warning("IP header unaligned, cannot use fast IP checksum");
		if (!ans)
			errh->message("(Try passing the configuration through %<click-align%>.)");
		_checked_aligned = true;
    }
#endif

    return 0;
}

Packet *
XIAIPEncap::simple_action(Packet *p_in)
{    
    const struct click_xia* hdr = p_in->xia_header();
    int last = hdr->last;
    if (last < 0)
    	last += hdr->dnode;
    const struct click_xia_xid_edge* edge = hdr->node[last].edge;
    const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p_in)];
    const int& idx = current_edge.idx;
    const struct click_xia_xid_node& node = hdr->node[idx];
  
    if (htonl(node.xid.type) != CLICK_XIA_XID_TYPE_IP)
        return 0;
    
    long storedip = *(long *)(node.xid.id + 16);
    _daddr.s_addr = storedip; 
  
    WritablePacket *p = p_in->push(sizeof(click_udp) + sizeof(click_ip));
    click_ip *ip = reinterpret_cast<click_ip *>(p->data());
    click_udp *udp = reinterpret_cast<click_udp *>(ip + 1);

#if !HAVE_INDIFFERENT_ALIGNMENT
    assert((uintptr_t)ip % 4 == 0);
#endif
    // set up IP header
    ip->ip_v = 4;
    ip->ip_hl = sizeof(click_ip) >> 2;
    ip->ip_len = htons(p->length());
    ip->ip_id = htons(_id.fetch_and_add(1));
    ip->ip_p = IP_PROTO_UDP;
    ip->ip_src = _saddr;
    if (_use_dst_anno)
        ip->ip_dst = p->dst_ip_anno();
    else {
        ip->ip_dst = _daddr;
        p->set_dst_ip_anno(IPAddress(_daddr));
    }
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_ttl = 250;
  
    ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    if (_aligned)
        ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
    else
        ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif

    p->set_ip_header(ip, sizeof(click_ip));
  
    // set up UDP header
    udp->uh_sport = _sport;
    udp->uh_dport = _dport;
    uint16_t len = p->length() - sizeof(click_ip);
    udp->uh_ulen = htons(len);
    udp->uh_sum = 0;

    if (_cksum) {
        unsigned csum = click_in_cksum((unsigned char *)udp, len);
        udp->uh_sum = click_in_cksum_pseudohdr(csum, ip, len);
    }
  
    return p;
}


String XIAIPEncap::read_handler(Element *e, void *thunk)
{
    XIAIPEncap *u = static_cast<XIAIPEncap *>(e);
    switch ((uintptr_t) thunk) {
		case 0:
			return IPAddress(u->_saddr).unparse();
		case 1:
			return String(ntohs(u->_sport));
		case 2:
			return IPAddress(u->_daddr).unparse();
		case 3:
			return String(ntohs(u->_dport));
		default:
			return String();
    }
}

void XIAIPEncap::add_handlers()
{
    add_read_handler("src", read_handler, (void *) 0);
    add_write_handler("src", reconfigure_keyword_handler, "0 SRC");
    add_read_handler("sport", read_handler, (void *) 1);
    add_write_handler("sport", reconfigure_keyword_handler, "1 SPORT");
    add_read_handler("dst", read_handler, (void *) 2);
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
    add_read_handler("dport", read_handler, (void *) 3);
    add_write_handler("dport", reconfigure_keyword_handler, "3 DPORT");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAIPEncap)
ELEMENT_MT_SAFE(XIAIPEncap)
