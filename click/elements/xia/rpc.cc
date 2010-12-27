#include <click/config.h>
#include "rpc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include "../../userlevel/xia.pb.h"
#include <string>
#include <assert.h>
#include <stdio.h>

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
  states = 0;
  return 0;
}

int
rpc::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _active = true;
  return 0;
}

/*Packet *
rpc::simple_action(Packet *p)
{   printf ("incoming packets: %s", (const char*) p->data());   

    return p;
    }*/

void 
rpc::push(int port, Packet *p)
{   
  GOOGLE_PROTOBUF_VERIFY_VERSION;  
  if (port%2 == 0)  {
    xia::msg_request msg;
    std::string data ((const char*)p->data(), p->length());
    //recover protobuf-based msg (just print, to coordinate with XIA encap)
    msg.ParseFromString(data);   
    printf ("AppID: %d, XID: %s, Type: %d\n", msg.appid(), msg.xid().c_str(), msg.type());   
    //forward to HID
    output(port).push(p);  
  }  
  else if (port%2 == 1)  {
    // reassembly *****?
     xia::msg_response msg2;
     std::string data_response;
     //construct protobuf-based msg
     msg2.set_appid(port/2);
     msg2.add_xid("00000000000000000000");
     msg2.set_data("hello");
     msg2.SerializeToString (&data_response);
     WritablePacket *p2 = WritablePacket::make (data_response.c_str(), data_response.length());
     output(port).push(p2);
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
    add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE | Handler::CHECKBOX | Handler::CALM, &_active);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(rpc)
ELEMENT_MT_SAFE(rpc)
