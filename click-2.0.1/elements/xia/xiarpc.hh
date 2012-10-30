#ifndef CLICK_XIARPC_HH
#define CLICK_XIARPC_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/xiapath.hh>
#include "../../userlevel/xia.pb.h"

CLICK_DECLS

class XIARPC : public Element { public:

    XIARPC();
    ~XIARPC();
  
  const char *class_name() const		{ return "XIARPC"; }
  const char *port_count() const		{ return "3/3"; }  
  const char *processing() const		{ return "a/h"; }  

  //void static_initialize();
  int initialize();
  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();
  void push(int, Packet *);

 private:
  int computeOutputPort (int, int);
  WritablePacket* generateXIAPacket (xia::msg &msg);

  void append(Packet *p);
  char* receive(size_t len);
  uint32_t desired_len;
  uint32_t nread;
  char * buffer;
  char * remaining_buffer;

  char* _buffer;
  size_t _buffer_size;
  size_t _buffer_capacity;
  uint32_t _message_len;

  XIAPath _local_addr;
};

CLICK_ENDDECLS
#endif
