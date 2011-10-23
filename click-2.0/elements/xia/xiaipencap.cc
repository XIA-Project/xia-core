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
{
}

XIAIPEncap::~XIAIPEncap()
{
}

int
XIAIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  click_ip iph;
  memset(&iph, 0, sizeof(click_ip));
  iph.ip_v = 4;
  iph.ip_hl = sizeof(click_ip) >> 2;
  iph.ip_ttl = 250;
  int proto = IP_XID_PROTO, tos = -1, dscp = -1;
  bool ce = false, df = false;
  String ect_str, dst_str;

    if (Args(conf, this, errh)
	.read("PROTO", NamedIntArg(NameInfo::T_IP_PROTO), proto)
	.read_mp("SRC", iph.ip_src)
	.read("DST", AnyArg(), dst_str)
	.read("TOS", tos)
	.read("TTL", iph.ip_ttl)
	.read("DSCP", dscp)
	.read("ECT", KeywordArg(), ect_str)
	.read("CE", ce)
	.read("DF", df)
	.complete() < 0)
	return -1;

  if (proto < 0 || proto > 255)
      return errh->error("bad IP protocol");
  iph.ip_p = proto;

  bool use_dst_anno = false;//dst_str == "DST_ANNO";
  /*if (use_dst_anno)
      iph.ip_dst.s_addr = 0;
  else if (!IPAddressArg().parse(dst_str, iph.ip_dst, this))
      return errh->error("DST argument should be IP address or 'DST_ANNO'");
  */

  int ect = 0;
  if (ect_str) {
    bool x;
    if (BoolArg().parse(ect_str, x))
      ect = x;
    else if (ect_str == "2")
      ect = 2;
    else
      return errh->error("bad ECT value '%s'", ect_str.c_str());
  }

  if (tos >= 0 && dscp >= 0)
    return errh->error("cannot set both TOS and DSCP");
  else if (tos >= 0 && (ect || ce))
    return errh->error("cannot set both TOS and ECN bits");
  if (tos >= 256 || tos < -1)
    return errh->error("TOS too large; max 255");
  else if (dscp >= 63 || dscp < -1)
    return errh->error("DSCP too large; max 63");
  if (ect && ce)
    return errh->error("can set at most one ECN option");

  if (tos >= 0)
    iph.ip_tos = tos;
  else if (dscp >= 0)
    iph.ip_tos = (dscp << 2);
  if (ect)
    iph.ip_tos |= (ect == 1 ? IP_ECN_ECT1 : IP_ECN_ECT2);
  if (ce)
    iph.ip_tos |= IP_ECN_CE;
  if (df)
    iph.ip_off |= htons(IP_DF);
  _iph = iph;

  // set the checksum field so we can calculate the checksum incrementally
#if HAVE_FAST_CHECKSUM
  _iph.ip_sum = ip_fast_csum((unsigned char *) &_iph, sizeof(click_ip) >> 2);
#else
  _iph.ip_sum = click_in_cksum((unsigned char *) &_iph, sizeof(click_ip));
#endif

  // store information about use_dst_anno in the otherwise useless
  // _iph.ip_len field
  _iph.ip_len = (use_dst_anno ? 1 : 0);

  return 0;
}


int
XIAIPEncap::initialize(ErrorHandler *)
{
  _id = 0;
  return 0;
}


inline void
XIAIPEncap::update_cksum(click_ip *ip, int off) const
{
#if HAVE_INDIFFERENT_ALIGNMENT
    click_update_in_cksum(&ip->ip_sum, 0, ((uint16_t *) ip)[off/2]);
#else
    const uint8_t *u = (const uint8_t *) ip;
# if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    click_update_in_cksum(&ip->ip_sum, 0, u[off]*256 + u[off+1]);
# else
    click_update_in_cksum(&ip->ip_sum, 0, u[off] + u[off+1]*256);
# endif
#endif
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

  if(htonl(node.xid.type) != CLICK_XIA_XID_TYPE_IP) {
    return 0;
  }
  
  long storedip = *(long*)&node.xid.id[16];
  _iph.ip_dst.s_addr = storedip; 
  
  WritablePacket *p = p_in->push(sizeof(click_ip));
  if (!p) return 0;

  click_ip *ip = reinterpret_cast<click_ip *>(p->data());
  memcpy(ip, &_iph, sizeof(click_ip));

  p->set_dst_ip_anno(IPAddress(ip->ip_dst));
  update_cksum(ip, 16);
  update_cksum(ip, 18);


  ip->ip_len = htons(p->length());
  ip->ip_id = htons(_id.fetch_and_add(1));
  update_cksum(ip, 2);
  update_cksum(ip, 4);


  p->set_ip_header(ip, sizeof(click_ip));

  return p;
}



String
XIAIPEncap::read_handler(Element *e, void *thunk)
{
  XIAIPEncap *ipe = static_cast<XIAIPEncap *>(e);
  switch ((intptr_t)thunk) {
    case 0:
      return IPAddress(ipe->_iph.ip_src).unparse();
    case 1:
      if (ipe->_iph.ip_len == 1)
	  return "DST_ANNO";
      else
	  return IPAddress(ipe->_iph.ip_dst).unparse();
    default:
      return "<error>";
  }
}

void
XIAIPEncap::add_handlers()
{
    add_read_handler("src", read_handler, 0, Handler::CALM);
    add_write_handler("src", reconfigure_keyword_handler, "1 SRC");
    add_read_handler("dst", read_handler, 1, Handler::CALM);
    add_write_handler("dst", reconfigure_keyword_handler, "2 DST");
}


CLICK_ENDDECLS
EXPORT_ELEMENT(XIAIPEncap)
ELEMENT_MT_SAFE(XIAIPEncap)
