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
  int count;
  Vector<String> ipencap;
  Vector<String> config_mine;
 
  /* IP encap configuration */ 
  for (Vector<String>::iterator it = conf.begin(); it< conf.end() ; it ++) {
    if ((*it).starts_with(String("COUNT"))) {
	click_chatter((*it).c_str());
	config_mine.push_back(*it);
	continue;
    }
    ipencap.push_back(*it);
  }
  int ret =  IPEncap::configure(ipencap, errh);
  if (ret<0) return ret;

  /* DynamicIPEncap configuation */
  if (cp_va_kparse(config_mine, this, errh,
                 "COUNT", 0, cpUnsigned, &count,
                 cpEnd) < 0)
      return -1;
  _max_count = count;
  _count = 0;
  return 0;
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
  if (_count == _max_count )
     _count = 0;
  _iph.ip_dst.s_addr = htonl(ntohl(_iph.ip_dst.s_addr)+_count);
  _count++;
  // if both are modified, it will confuse fdir atr's hashing
  //_iph.ip_src.s_addr = htonl(ntohl(_iph.ip_src.s_addr)+1);
  return p_out;
}
CLICK_ENDDECLS
EXPORT_ELEMENT(DynamicIPEncap)
ELEMENT_MT_SAFE(DynamicIPEncap)
