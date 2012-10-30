#ifndef CLICK_XIACACHE_HH
#define CLICK_XIACACHE_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>
#include "xiacontentmodule.hh"

#if CLICK_USERLEVEL
#include <list>
#include <string.h>
#include <stdio.h>
#include <iostream>
#endif

CLICK_DECLS

/**
This is a fork of XIAtransport

XIATransport:   input port[0]:  get CID request or reponse from network, should be connect to RouteEngine
output[0] : if the cache has the chunk, it will serve the CID request by pushing chunk pkts to RouteEngine
input port[1]:  connect with RPC, in server, the RPC will pushCID into cache before serve it.
output port[1]: connect with RPC, in client, when a chunk is complete, cache will push it to RPC (higher level)
*/

class XIAContentModule;    

class XIACache : public Element { 
  public:
    XIACache();
    ~XIACache();
    const char *class_name() const		{ return "XIACache"; }
    const char *port_count() const		{ return "2/2"; }
    const char *processing() const		{ return PUSH; }
    int configure(Vector<String> &, ErrorHandler *);         
    void push(int port, Packet *);            
    XID local_hid() { return _local_hid; };
    XIAPath local_addr() { return _local_addr; };
    void add_handlers();
    static int write_param(const String &, Element *, void *vparam, ErrorHandler *);

  private:
    uint32_t _cid_type;
    XID _local_hid;
    XIAPath _local_addr;
    XIAContentModule* _content_module;

};

CLICK_ENDDECLS

#endif




