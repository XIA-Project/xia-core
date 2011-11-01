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
#include "xiaxidroutetable.hh"
#include <clicknet/udp.h>
#include <click/string.hh>
#if CLICK_USERLEVEL
#include <list>
#include <stdio.h>
#include <iostream>

#include "../../userlevel/xia.pb.h"

#endif

#define CLICKCONTROLPORT 5001
//#define CLICKOPENPORT 5001
#define CLICKBINDPORT 5002
#define CLICKCLOSEPORT 5003
#define CLICKCONNECTPORT 5004
#define CLICKACCEPTPORT 5005

#define CLICKPUTCIDPORT 10002
#define CLICKSENDTOPORT 10001
#define CLICKDATAPORT 10000


CLICK_DECLS

/**
XUDP:   
input port[0]:  control port
input port[1]:  Socket Rx data port
input port[2]:  Network Rx data port
input port[3]:  in from cache

output[3]: To cache for putCID
output[2]: Network Tx data port 
output[1]: Socket Tx data port

Might need other things to handle chunking
*/

class XIAContentModule;    

class XUDP : public Element { 
  public:
    XUDP();
    ~XUDP();
    const char *class_name() const		{ return "XUDP"; }
    const char *port_count() const		{ return "4/4"; }
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
    bool isConnected;

    // protobuf message
    xia::XSocketMsg xia_socket_msg;
    //enum xia_socket_msg::XSocketMsgType type;
    
    Packet* UDPIPEncap(Packet *, int,int);
    
    struct DAGinfo{
    DAGinfo(): port(0), isConnected(false), initialized(false) {};
    unsigned short port;
    XID xid;
    XIAPath src_path;
    XIAPath dst_path;
    int nxt;
    int last;
    uint8_t hlim;
    bool isConnected;
    bool initialized;
    String sdag;
    String ddag;
    } ;
    
    HashTable<XID, unsigned short> XIDtoPort;
    HashTable<unsigned short, DAGinfo> portToDAGinfo;
    HashTable<unsigned short, int> portRxSeqNo;
    HashTable<unsigned short, int> portTxSeqNo;
    
    struct in_addr _CLICKaddr;
    struct in_addr _APIaddr;
    atomic_uint32_t _id;
    bool _cksum;
    XIAXIDRouteTable *_routeTable;
    
    //modify routing table
    void addRoute(const XID &sid) {
        String cmd=sid.unparse()+" 4";
        HandlerCall::call_write(_routeTable, "add", cmd);
    }   
        
    void delRoute(const XID &sid) {
        String cmd= sid.unparse();
        HandlerCall::call_write(_routeTable, "remove", cmd);
    }

};


CLICK_ENDDECLS

#endif




