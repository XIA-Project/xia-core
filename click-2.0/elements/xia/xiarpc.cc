#include <click/config.h>
#include "xiarpc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <click/xiaheader.hh>
#include <click/xiacontentheader.hh>


#include <iostream>

CLICK_DECLS

XIARPC::XIARPC() : _buffer(NULL), _buffer_size(0), _buffer_capacity(0), _message_len(0)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;  
}

XIARPC::~XIARPC()
{
    free(_buffer);
    _buffer = NULL;
}

int
XIARPC::initialize()
{
  return 0;
}

int
XIARPC::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &_local_addr,
                   cpEnd) < 0)
        return -1;
  return 0;
}

int 
XIARPC::computeOutputPort (int inputPort, int type) 
{  if (inputPort == 0 && type == xia::msg::CONNECTSID) 
      return 1;
   else if (inputPort == 0 && type == xia::msg::GETCID) 
      return 1;
   else if (inputPort == 0 && type == xia::msg::PUTCID) 
      return 2;
   else if (inputPort == 0 && type == xia::msg::SERVESID) 
      return 1;
   else if (inputPort == 1 || inputPort == 2)
      return 0;   
   else  {
     printf ("inport: %d, type: %d\n" ,inputPort, type);
      assert(false);
   }
}



WritablePacket*
XIARPC::generateXIAPacket (xia::msg &msg_protobuf)  {
  int pktPayloadSize = msg_protobuf.payload().length();

  WritablePacket *p_click = WritablePacket::make (256, msg_protobuf.payload().c_str(), pktPayloadSize, 0);  // constructing payload
  if (!p_click) {
    printf ("error: construct a click packet\n");  
    return 0;
  }    
  // src and dest XIA paths
  String source (msg_protobuf.xiapath_src().c_str());
  String dest (msg_protobuf.xiapath_dst().c_str());
    
  XIAPath src_path;
  src_path.parse_re(source);  
  XIAPath dst_path;
  dst_path.parse_re(dest);    

  XIAHeaderEncap enc;
    
  //printf ("dest:%s, src: %s\n", dest.c_str(), source.c_str());
  enc.set_dst_path(dst_path);
  enc.set_src_path(src_path);
  enc.set_plen (pktPayloadSize); 


  // extention header enc
  //if (msg_protobuf.type == xia::msg::CONNECTSID) 
  
  if (msg_protobuf.type() == xia::msg::GETCID || msg_protobuf.type() == xia::msg::PUTCID)  {
    enc.set_nxt(CLICK_XIA_NXT_CID);
    int chunkSize = pktPayloadSize;
    ContentHeaderEncap  contenth(0, 0, pktPayloadSize, chunkSize);
    p_click = contenth.encap(p_click); 
  }
  return enc.encap(p_click, false);
}


void XIARPC::append(Packet *p)
{  
    size_t required_size = _buffer_size + p->length();
    if (required_size > _buffer_capacity)
      {
	if (required_size == 0)
	  _buffer_capacity = 1;
	else 
	  {
	    //next power of 2
	    _buffer_capacity = required_size - 1;
	    for (size_t i = 1; i < sizeof(_buffer_capacity) * 8; i <<= 1)
	      _buffer_capacity = _buffer_capacity | _buffer_capacity >> i;
	    _buffer_capacity++;
	  }
	_buffer = (char*)realloc(_buffer, _buffer_capacity);
	assert(_buffer);
      }
    memcpy(_buffer + _buffer_size, p->data(), p->length());
    _buffer_size += p->length();
   
}

char* XIARPC::receive(size_t len)
{  
    if (_buffer_size >= len)
    {
        char* ret = (char*)malloc(len);
        assert(ret);

        memcpy(ret, _buffer, len);

        _buffer_size -= len;
        memmove(_buffer, _buffer + len, _buffer_size);

        return ret;
    }
    else
        return NULL;
}

void 
XIARPC::push(int port, Packet *p)
{   
  if (port == 0)  {                                   // from socket
    click_chatter("RPC at %s: packet from socket", _local_addr.unparse(this).c_str());

    append(p);
    p->kill();
    p = NULL;
 
    if (_message_len == 0)
    {
        char* size = receive(4);
        if (size == NULL)
            return;
        _message_len = ntohl(*(uint32_t*)size);
        free(size);
        assert(_message_len != 0);
    }

    click_chatter("msg len %d\n", _message_len); 

    char* message = receive(_message_len); 
    if (message == NULL) {
        printf ("msg NULL\n");
	return;
    }
    std::string data (message, _message_len);

    /*
    std::string s;
    for (int i=0;i<desired_len;i++) {
	char buf[3];
	sprintf(buf, "%02hhx", (unsigned char)(message[i]));
	s += buf;
	}*/

    free(message);
    _message_len = 0;

    //click_chatter("%s", s.c_str());
    
    // if you have read len bytes
    xia::msg msg_protobuf;
    msg_protobuf.ParseFromString(data);   
     

    WritablePacket* p_xia = generateXIAPacket (msg_protobuf);
    //printf ("type: %d\npayload: %s", msg_protobuf.type(), msg_protobuf.payload().c_str());
    
    int outputPort = computeOutputPort(port, msg_protobuf.type()); //computeOutputPort(port);
    output(outputPort).push(p_xia);  
  }  
  else  if (port == 1) {   // from route Engine
    click_chatter("RPC at %s: packet from network", _local_addr.unparse(this).c_str());
    //read xia header
    XIAHeader xiah(p);
    std::string payload((const char*)xiah.payload(), xiah.plen());
    p->kill();
    p = NULL;

    //construct protobuf-based msg
    //printf("RPC received payload size: %d\n", xiah.plen());
    xia::msg msg_protobuf;
    std::string data;
     
    if (xiah.plen() == 0) 
      msg_protobuf.set_type(xia::msg::CONNECTSID);  //todo: use SID part of src addr to tell
    else
      msg_protobuf.set_type(xia::msg::SERVESID);
    msg_protobuf.set_payload(payload);
    msg_protobuf.set_xiapath_dst("AD:1000000000000000000000000000000000000001 HID:0000000000000000000000000000000000000001");
    msg_protobuf.set_xiapath_src("AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000");

    msg_protobuf.SerializeToString (&data);
    WritablePacket *p2 = WritablePacket::make (256, data.c_str(), data.length(), 0)->push(4);
    int32_t size = htonl(data.length());
    //printf ("size of connectsid message: %d(htonl), %d\n", size, data.length());
    memcpy(p2->data(), &size , 4); 
    //WritablePacket *p2 = WritablePacket::make (256, payload, xiah.plen(),0);
    int outputPort = 0; //computeOutputPort(port);
    output(outputPort).push(p2);
  }
  else  {   // from cache
    assert (port ==2);
    printf ("interface2: from cache\n");

    //read xia header
    XIAHeader xiah(p);
    std::string payload((char*)xiah.payload(), xiah.plen());

    //printf("RPC received payload size: %d\n", xiah.plen());
    //construct protobuf-based msg
    xia::msg msg_protobuf;
    std::string data_response;
	 
    msg_protobuf.set_payload(payload);
    msg_protobuf.SerializeToString (&data_response);
    WritablePacket *p2 = WritablePacket::make (256, data_response.c_str(), data_response.length(), 0)->push(4);
    int32_t size = ntohl(data_response.length());
    memcpy(p2->data(), &size , 4); 
std::cout<<"In RPC"<<std::endl;
std::cout<<"pushed payload: "<<p2->data()<<std::endl;
    output(0).push(p2);
    p->kill();
  }
}
/*
Packet *
XIARPC::pull (int port)
{ Packet *p = input(port).pull();
  if (p)
    p = simple_action(p);
  return p;
  }*/

void
XIARPC::add_handlers()
{
   
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIARPC)
ELEMENT_REQUIRES(userlevel)
ELEMENT_MT_SAFE(XIARPC)
ELEMENT_LIBS(-lprotobuf)
