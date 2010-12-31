#include <click/config.h>
#include "rpc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "../../userlevel/xia.pb.h"
#include <string>
#include <assert.h>
#include <stdio.h>
#include <click/xiaheader.hh>

CLICK_DECLS

rpc::rpc()
{
  
}

rpc::~rpc()
{
}

int
rpc::initialize()
{
  //  states = 0;
  return 0;
}

int
rpc::configure()
{
  // _active = true;
  return 0;
}

/*Packet *
rpc::simple_action(Packet *p)
{   printf ("incoming packets: %s", (const char*) p->data());   

    return p;
    }*/




int 
rpc::computeOutputPort (int inputPort) 
{  if (inputPort != 0) 
      return 0;
   else 
      return 1;  //might change if multiplexing apps 
}

void 
rpc::push(int port, Packet *p)
{   
  GOOGLE_PROTOBUF_VERIFY_VERSION;  
  if (port != 0)  {                                   // from socket
    xia::msg_request msg;
    std::string data ((const char*)p->data(), p->length());
    msg.ParseFromString(data);   
   
    // make a header
    //XIAHeaderEncap encp (&header); // pass in reference of header obj

    WritablePacket *p_click = WritablePacket::make (256, msg.payload().c_str(), msg.payload().length(), 0);  // constructing payload
    if (!p_click) printf ("error: construct a click packet\n");  
    
    // src and dest XIA paths
    String source (msg.xiapath_src().c_str());
    String dest (msg.xiapath_dst().c_str());
    
    XIAPath src_path;
    src_path.parse_re(source);  
    XIAPath dst_path;
    dst_path.parse_re(dest);    

    XIAHeaderEncap enc;
    
    enc.set_dst_path(dst_path);
    enc.set_src_path(src_path);
    
    //printf ("payload sent: %s\n", (char*) p_click->data()); // to be removed
    WritablePacket* p_xia = enc.encap(p_click);
    //const click_xia *xiah = p_xia->xia_header();
    //int hrlen = XIAHeader::hdr_size(xiah->dnode + xiah->snode);
    //printf ("hdrlen: %d\n", hrlen);


    int outputPort = computeOutputPort(port);
    output(outputPort).push(p_xia);  
  }  
  else if (port == 0)  {   // from network
    //read xia header
    XIAHeader xiah(p);
    std::string payload((char*)xiah.hdr()+xiah.hdr_size(), xiah.plen());

    printf("RPC1 received: payload:%s,payload: %s,pay_len:%d, headersize: %d\n", (char*)xiah.hdr()+xiah.hdr_size(), xiah.payload(), xiah.plen(), xiah.hdr_size());
    //construct protobuf-based msg
    xia::msg_request msg_protobuf;
    std::string data_response;
	   // msg2.set_appid(port/2);
	   //     msg2.add_xid("00000000000000000000");
     msg_protobuf.set_payload(payload);
     msg_protobuf.SerializeToString (&data_response);
     WritablePacket *p2 = WritablePacket::make (256, data_response.c_str(), data_response.length(), 0);
    
	   //WritablePacket *p2 = WritablePacket::make (256, payload, xiah.plen(),0);
     int outputPort = computeOutputPort(port);
     output(outputPort).push(p2);
  }

}
/*
Packet *
rpc::pull (int port) 
{ Packet *p = input(port).pull();
  if (p)
    p = simple_action(p);
  return p;
  }*/

void
rpc::add_handlers()
{
   
}

CLICK_ENDDECLS
EXPORT_ELEMENT(rpc)
ELEMENT_MT_SAFE(rpc)
