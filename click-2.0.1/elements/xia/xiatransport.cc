#include <click/config.h>
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
XIATransport::XIATransport()
{
    cp_xid_type("CID", &_cid_type);   
    // oldPartial=contentTable;
    _content_module = new XIAContentModule(this);
}

XIATransport::~XIATransport()
{
    delete _content_module;
}

int 
XIATransport::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element* routing_table_elem;
    XIAPath local_addr;
    bool cache_content_from_network =true;

    if (cp_va_kparse(conf, this, errh,
		"LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &local_addr,
		"ROUTETABLENAME", cpkP+cpkM, cpElement, &routing_table_elem,
		"CACHE_CONTENT_FROM_NETWORK", cpkP, cpBool, &cache_content_from_network,
		cpEnd) < 0)
	return -1;   
#if USERLEVEL
    _content_module->_routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
#else
    _content_module->_routeTable = reinterpret_cast<XIAXIDRouteTable*>(routing_table_elem);
#endif
    _local_addr = local_addr;
    _local_hid = local_addr.xid(local_addr.destination_node());

    _content_module->_cache_content_from_network = true;
    /*
       std::cout<<"Route Table Name: "<<routing_table_name.c_str()<<std::endl;
       if(routeTable==NULL) 
            std::cout<<"NULL routeTable"<<std::endl;
       if(ad_given)
       {
           std::cout<<"ad: "<<sad.c_str()<<std::endl;
           ad.parse(sad);
           std::cout<<ad.unparse().c_str()<<std::endl;
       }
       if(hid_given)
       {
           std::cout<<"hid: "<<shid.c_str()<<std::endl;
           hid.parse(shid);
           std::cout<<hid.unparse().c_str()<<std::endl;
       }
       std::cout<<"pkt size: "<<PKTSIZE<<std::endl;
     */
    return 0;
}



void XIATransport::push(int port, Packet *p)
{
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


    if(src_xid_type==_cid_type)  //store, this is chunk response
   	_content_module->cache_incoming(p, srcID, dstID, port);
    else if(dst_xid_type==_cid_type)  //look_up,  chunk request
   	_content_module->process_request(p, srcID, dstID);
    else
    {
	p->kill();
#if DEBUG_PACKET
	click_chatter("src and dst are not CID");
#endif
    }
}



CLICK_ENDDECLS
EXPORT_ELEMENT(XIATransport)
//ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(XIAContentModule)
ELEMENT_MT_SAFE(XIATransport)
