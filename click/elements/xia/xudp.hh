#ifndef CLICK_XUDP_HH
#define CLICK_XUDP_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>
#include <clicknet/xia.h>
#include "xiacontentmodule.hh"
#include <clicknet/udp.h>
#include <click/string.hh>
#if CLICK_USERLEVEL
#include <list>
#include <stdio.h>
#include <iostream>

#endif

#define DEBUG_PACKET 1

#define CLICKOPENPORT 1
#define CLICKBINDPORT 2
#define CLICKCLOSEPORT 3
#define CLICKCONNECTPORT 4
#define CLICKCONTROLPORT 5

#define CLICKDATAPORT 10
#define XIDsize

CLICK_DECLS

/**
XUDP:   
input port[0]:  control port
input port[1]:  Socket Rx data port
input port[2]:  Network Rx data port


output[0]: Network Tx data port 
output[1]: Socket Tx data port
Might need other things to handle chunking
*/

class XIAContentModule;    

class XUDP : public Element { 
  public:
    XUDP();
    ~XUDP();
    const char *class_name() const		{ return "XUDP"; }
    const char *port_count() const		{ return "3/2"; }
    const char *processing() const		{ return PUSH; }
    int configure(Vector<String> &, ErrorHandler *);         
    void push(int port, Packet *);            
    XID local_hid() { return _local_hid; };
    XIAPath local_addr() { return _local_addr; };

  private:
    uint32_t _cid_type;
    XID _local_hid;
    XIAPath _local_addr;
    
    Packet* UDPIPEncap(Packet *, int);
    
    struct DAGinfo{
    unsigned short port;
    XID xid;
    XIAPath src_path;
    XIAPath dst_path;
    int nxt;
    int last;
    uint8_t hlim;
    } ;
    
    HashTable<XID, unsigned short> XIDtoPort;
    HashTable<unsigned short, DAGinfo> portToDAGinfo;
    HashTable<unsigned short, int> portRxSeqNo;
    HashTable<unsigned short, int> portTxSeqNo;
    
    struct in_addr _CLICKaddr;
    struct in_addr _APIaddr;
    atomic_uint32_t _id;
    bool _cksum;

};

CLICK_ENDDECLS

#endif




