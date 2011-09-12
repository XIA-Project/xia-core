#include <click/config.h>
#include "dynamicipencap.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

DynamicIPEncap::DynamicIPEncap() 
{
}

DynamicIPEncap::~DynamicIPEncap()
{
}

int
DynamicIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return IPEncap::configure(conf, errh);
}

Packet *
DynamicIPEncap::simple_action(Packet *p_in)
{
  Packet *p_out=  IPEncap::simple_action(p_in);
  click_ip *ip = reinterpret_cast<click_ip *>(((WritablePacket*)p_out)->data());
  ip->ip_sum = 0;
#if HAVE_FAST_CHECKSUM
  ip->ip_sum = ip_fast_csum((unsigned char *) ip, sizeof(click_ip) >> 2);
#else
  ip->ip_sum = click_in_cksum((unsigned char *) ip, sizeof(click_ip));
#endif
  _iph.ip_dst.s_addr = htonl(ntohl(_iph.ip_dst.s_addr)+1);
  // if both are modified, it will confuse fdir atr's hashing
  //_iph.ip_src.s_addr = htonl(ntohl(_iph.ip_src.s_addr)+1);
  return p_out;
}
CLICK_ENDDECLS
EXPORT_ELEMENT(DynamicIPEncap)
ELEMENT_MT_SAFE(DynamicIPEncap)
