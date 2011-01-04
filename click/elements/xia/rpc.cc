#include <click/config.h>
#include "rpc.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <click/xiaheader.hh>

CLICK_DECLS

rpc::rpc() : desired_len(0), nread(0), buffer(0),  remaining_buffer(0), message_len(0)
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

WritablePacket*
rpc::generateXIAPacket (xia::msg_request &msg)  {
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

char* rpc::receive(Packet *p)
{
	if (p)
	{
		printf ("desired_len: %d  nread: %d  payload length: %d\n", desired_len, nread, p->length());
		char* new_remaining_buf = (char*)malloc(nread + p->length());
		if (remaining_buffer)
			memcpy(new_remaining_buf, remaining_buffer, nread);
		memcpy(new_remaining_buf + nread, p->data(), p->length());
		nread += p->length();
		free(remaining_buffer);
		remaining_buffer = new_remaining_buf;
		new_remaining_buf = NULL;
	}

	if (nread >= desired_len)
	{
		buffer = (char*)malloc(desired_len);
		memcpy(buffer, remaining_buffer, desired_len);
		memmove(remaining_buffer, remaining_buffer + desired_len, nread - desired_len);
		nread -= desired_len;
		if (nread == 0)
		{
			free(remaining_buffer);
			remaining_buffer = NULL;
		}
		return buffer;
	}
	return NULL;
	
/*
   click_chatter("p->length %d", p->length());
   if (buffer==NULL)  {
       buffer = (char *)malloc(desired_len);  
       if (remaining_buffer) {
           click_chatter("desired len %d nread %d", desired_len, nremaining);
 	   if (desired_len<nread) {
	      // return desired len
              // keep the rest in remaining buffer
              memcpy(buffer, remaining_buffer, desired_len);
              memmove(remaining_buffer, remaining_buffer + desired_len, nremaining - desired_len);
              nremaining -= desired_len;
	      char * ret = buffer;
              buffer = NULL;
	      return ret;
	   }
           else
           {
              memcpy(buffer, remaining_buffer, nread);
              nread = 0;
           }
       }
       free(remaining_buffer);
       remaining_buffer =NULL;
   }
   uint32_t copied_len = 0;
   if (p->length()> desired_len-nread)
       copied_len = desired_len-nread;
   else
       copied_len = p->length();

   memcpy(buffer, p->data(), copied_len ); 
   //nread+= copied_len;
   if (nread==desired_len)  {
      char * ret = buffer;
      if (p->length()-copied_len!=0) {
	      remaining_buffer = (char*)malloc(p->length()-copied_len);
	      memcpy(remaining_buffer, p->data()+copied_len, p->length()-copied_len);
      }
      buffer = NULL;
      nread= p->length()-copied_len;
      click_chatter("nread %d",  nread);
      return ret;
   }
   return NULL;
*/
}

void 
rpc::push(int port, Packet *p)
{   
  GOOGLE_PROTOBUF_VERIFY_VERSION;  
  if (port != 0)  {                                   // from socket
    printf ("interface1\n");
    xia::msg_request msg;

    desired_len = 0;
    free(receive(p));		// enqueue payload to buffer
    p->kill();
 
    if (message_len == 0)
    {
	    desired_len = 4;
	    char* size = receive (NULL);
	    if (size==NULL)  {
		return;
	    }
	    message_len = ntohl(*(uint32_t*)size);
	    free(size);
    }

    desired_len = message_len;
    
    click_chatter("desired len %d\n", desired_len); 

    char* message = receive(NULL); 
    if (message == NULL)  {
	return;
    }

    message_len = 0;

    p = NULL;

    std::string data (message, desired_len);

    std::string s;
    for (int i=0;i<desired_len;i++) {
	char buf[3];
	sprintf(buf, "%02hhx", (unsigned char)(message[i]));
	s += buf;
    }
    //click_chatter("%s", s.c_str());
    
    free(message);
    // if you have read len bytes
    msg.ParseFromString(data);   
     
    // make a header
    //XIAHeaderEncap encp (&header); // pass in reference of header obj

    WritablePacket* p_xia = generateXIAPacket (msg);
    //const click_xia *xiah = p_xia->xia_header();
    //int hrlen = XIAHeader::hdr_size(xiah->dnode + xiah->snode);
    //printf ("hdrlen: %d\n", hrlen);


    int outputPort = computeOutputPort(port);
    output(outputPort).push(p_xia);  
  }  
  else if (port == 0)  {   // from network
    printf ("interface0\n");

    //read xia header
    XIAHeader xiah(p);
    std::string payload((char*)xiah.payload(), xiah.plen());

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
