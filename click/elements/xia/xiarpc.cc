#include <click/config.h>
#include "xiarpc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <click/xiaheader.hh>

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
  //  states = 0;
  return 0;
}

int
XIARPC::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "LOCAL_ADDR", cpkP+cpkM, cpXIAPath, &_local_addr,
                   cpEnd) < 0)
        return -1;
  // _active = true;
  return 0;
}

/*Packet *
XIARPC::simple_action(Packet *p)
{   printf ("incoming packets: %s", (const char*) p->data());   

    return p;
    }*/




int 
XIARPC::computeOutputPort (int inputPort) 
{  if (inputPort != 0) 
      return 0;
   else 
      return 1;  //might change if multiplexing apps 
}

WritablePacket*
XIARPC::generateXIAPacket (xia::msg_request &msg)  {
    WritablePacket *p_click = WritablePacket::make (256, msg.payload().c_str(), msg.payload().length(), 0);  // constructing payload
    if (!p_click) {
      printf ("error: construct a click packet\n");  
      return 0;
    }    
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
    return enc.encap(p_click);
}

void XIARPC::append(Packet *p)
{
    if (_buffer_size + p->length() > _buffer_capacity)
    {
        _buffer_capacity <<= 1;
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
    xia::msg_request msg;

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

    click_chatter("desired len %d\n", _message_len); 

    char* message = receive(NULL); 
    if (message == NULL)
	return;

    std::string data (message, _message_len);

    /*
    std::string s;
    for (int i=0;i<desired_len;i++) {
	char buf[3];
	sprintf(buf, "%02hhx", (unsigned char)(message[i]));
	s += buf;
    }
    */

    free(message);
    _message_len = 0;

    //click_chatter("%s", s.c_str());
    
    // if you have read len bytes
    msg.ParseFromString(data);   
     
    // make a header
    //XIAHeaderEncap encp (&header); // pass in reference of header obj

    WritablePacket* p_xia = generateXIAPacket (msg);
    //const click_xia *xiah = p_xia->xia_header();
    //int hrlen = XIAHeader::hdr_size(xiah->dnode + xiah->snode);
    //printf ("hdrlen: %d\n", hrlen);


    int outputPort = 1; //computeOutputPort(port);
    output(outputPort).push(p_xia);  
  }  
  else {
    click_chatter("RPC at %s: packet from network", _local_addr.unparse(this).c_str());

    //read xia header
    XIAHeader xiah(p);
    std::string payload((const char*)xiah.payload(), xiah.plen());
    p->kill();
    p = NULL;

    //printf("RPC received payload:\n%spay_len: %d\nheadersize: %d\n", (char*)xiah.payload(), xiah.plen(), xiah.hdr_size());
    //construct protobuf-based msg
    xia::msg_request msg_protobuf;
    std::string data_response;
	   // msg2.set_appid(port/2);
	   //     msg2.add_xid("00000000000000000000");
     msg_protobuf.set_payload(payload);
     msg_protobuf.SerializeToString (&data_response);
     WritablePacket *p2 = WritablePacket::make (256, data_response.c_str(), data_response.length(), 0)->push(4);
     int32_t size = htonl(data_response.length());
     memcpy(p2->data(), &size , 4); 
     
    
	   //WritablePacket *p2 = WritablePacket::make (256, payload, xiah.plen(),0);
     int outputPort = 0; //computeOutputPort(port);
     output(outputPort).push(p2);
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
ELEMENT_MT_SAFE(XIARPC)
