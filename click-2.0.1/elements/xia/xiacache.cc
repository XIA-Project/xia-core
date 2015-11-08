#include <click/config.h>
#include "xiacache.hh"
#include "xiatransport.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/xiacontentheader.hh>
#include <click/string.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/vector.hh>
#include <click/xid.hh>


CLICK_DECLS

XIACache::XIACache() {
  cp_xid_type("CID", &_cid_type);   
  // oldPartial=contentTable;
  XIATransport *xt;
#if USERLEVEL
  xt = dynamic_cast<XIATransport*>(this);
#else
  xt = reinterpret_cast<XIATransport*>(this);
#endif

  _content_module = new XIAContentModule(xt);
}

XIACache::~XIACache() { delete _content_module; }

int XIACache::configure(Vector<String> &conf, ErrorHandler *errh) {
  Element* routing_table_elem;
  XIAPath local_addr;
  int pkt_size = 0;
  int malicious = 0;
  bool cache_content_from_network = true;

  if (cp_va_kparse(conf, this, errh,
    "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
    "ROUTETABLENAME", cpkP+cpkM, cpElement, &routing_table_elem,
    "CACHE_CONTENT_FROM_NETWORK", cpkP, cpBool, &cache_content_from_network,
    "PACKET_SIZE", 0, cpInteger, &pkt_size,
    "MALICIOUS", 0, cpInteger, &malicious, cpEnd) < 0)
    return -1;   

  // Tell the content module whether or not it is malicious
  _content_module->malicious = malicious;

#if USERLEVEL
  _content_module->_routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
#else
  _content_module->_routeTable = reinterpret_cast<XIAXIDRouteTable*>(routing_table_elem);
#endif
  _local_addr = local_addr;
  _local_hid = local_addr.xid(local_addr.destination_node());

  if (pkt_size) 
    XIAContentModule::PKTSIZE = pkt_size;

  _content_module->_cache_content_from_network = cache_content_from_network;

  /*
    std::cout<<"Route Table Name: "<<routing_table_name.c_str()<<std::endl;
    if(routeTable==NULL) 
      std::cout<<"NULL routeTable"<<std::endl;
    if(ad_given) {
      std::cout<<"ad: "<<sad.c_str()<<std::endl;
      ad.parse(sad);
      std::cout<<ad.unparse().c_str()<<std::endl;
    }
    if(hid_given) {
      std::cout<<"hid: "<<shid.c_str()<<std::endl;
      hid.parse(shid);
      std::cout<<hid.unparse().c_str()<<std::endl;
    }
    std::cout<<"pkt size: "<<PKTSIZE<<std::endl;
  */
  return 0;
}

void XIACache::push(int port, Packet *p) {
  
  // click_chatter(">> In XIACAche port: %d", port);
  const struct click_xia* hdr = p->xia_header();

  if (!hdr) return;
  if (hdr->dnode == 0 || hdr->snode == 0) return;

  // Parse XIA source and destination
  struct click_xia_xid __dstID =  hdr->node[hdr->dnode - 1].xid;
  uint32_t dst_xid_type = __dstID.type;
  struct click_xia_xid __srcID = hdr->node[hdr->dnode + hdr->snode - 1].xid;
  uint32_t src_xid_type = __srcID.type;

  XID dstID(__dstID);
  XID srcID(__srcID);

  // chunk response, store
  if (src_xid_type ==_cid_type) {
    for (int i = 0; i < hdr->dnode + hdr->snode; i++) {
      XID dsthdr(hdr->node[i].xid);
      // click_chatter("PUT/REMOVE/FWD: dnode: %d, snode: %d,  i: %d, ID -> %s\n", hdr->dnode , hdr->snode, i, dsthdr.unparse().c_str() );
    }
  
    __dstID = hdr->node[hdr->dnode - 2].xid;
    XID dstHID(__dstID);
    // click_chatter("dstHID: %s\n", dstHID.unparse().c_str() );
    _content_module->cache_incoming(p, srcID, dstHID, port); // This may generate chunk response to port 1 (end-host/application)
  }
  // chunk request, look up
  else if (dst_xid_type == _cid_type) {
    for (int i = 0; i < hdr->dnode + hdr->snode; i++) {
      XID dsthdr(hdr->node[i].xid);
      // click_chatter("REQUEST: dnode: %d, snode: %d,  i: %d, ID -> %s\n", hdr->dnode , hdr->snode, i, dsthdr.unparse().c_str() );
    }
  
    __srcID = hdr->node[hdr->dnode + hdr->snode - 2].xid;
    XID srcHID(__srcID);
    // click_chatter("srcHID: %s\n", srcHID.unparse().c_str() );

    /* this sends chunk response to output 0 (network) or to 1 (app, if the request came from application and content is locally cached)  */
    _content_module->process_request(p, srcHID, dstID);
  }
  else {
    p->kill();
    
#if DEBUG_PACKET
  click_chatter("src and dst are not CID");
#endif
  }
}

int XIACache::set_malicious(int m) {
  _content_module->malicious = m;
  return 0;
}

int XIACache::get_malicious() {
  return _content_module->malicious;
}

enum {H_MOVE, MALICIOUS};

int XIACache::write_param(const String &conf, Element *e, void *vparam, ErrorHandler *errh) {
  XIACache *f = static_cast<XIACache *>(e);
  switch(reinterpret_cast<intptr_t>(vparam)) {
    case H_MOVE: {
      XIAPath local_addr;
      if (cp_va_kparse(conf, f, errh, "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr, cpEnd) < 0)
        return -1;
      f->_local_addr = local_addr;
      //click_chatter("%s",local_addr.unparse().c_str());
      f->_local_hid = local_addr.xid(local_addr.destination_node());
    } break;

    case MALICIOUS: {
      f->set_malicious(atoi(conf.c_str()));
    } break;
    
    default: 
      break;
  }
  return 0;
}

String XIACache::read_handler(Element *e, void *thunk) {
  XIACache *c = (XIACache *) e;
  switch ((intptr_t)thunk) {
    case MALICIOUS:
      return String(c->get_malicious());

    default:
      return "<error>";
    }
}

void XIACache::add_handlers() {
  add_write_handler("local_addr", write_param, (void *)H_MOVE);
  add_write_handler("malicious", write_param, (void*)MALICIOUS);
  add_read_handler("malicious", read_handler, (void*)MALICIOUS);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(XIACache)
//ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(XIAContentModule)
ELEMENT_MT_SAFE(XIACache)
