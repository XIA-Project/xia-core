#ifndef CLICK_XIAROUTERCACHE_HH
#define CLICK_XIAROUTERCACHE_HH

#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>

#if CLICK_USERLAND
#include <list>
#include <string.h>
#include <stdio.h>
#include <iostream>
#endif

#define CACHESIZE 1024*1024*1024    //in router 
#define CLIENTCACHE
#define PACKETSIZE 64*1024		

CLICK_DECLS

class CPart{
  public: 
    CPart(unsigned int,unsigned int);
    ~CPart(){}
    unsigned int offset;
    unsigned int length;
};

class CChunk{
  public:
    CChunk(XID, int);
    ~CChunk();
    int fill(const unsigned char* , unsigned int, unsigned int);
    bool full();
    unsigned int GetSize()
    {
	return size;
    }
    char* GetPayload()
    {
	return payload;
    }
  private:
    XID xid;
    bool complete;
    unsigned int size;
    char* payload;
    std::list<CPart> parts;
    
    void Merge(std::list<CPart>::iterator);
};

/**
XIARouterCache:   input port[0]:  get CID request or reponse from network, should be connect to RouteEngine
output[0] : if the cache has the chunk, it will serve the CID request by pushing chunk pkts to RouteEngine
input port[1]:  connect with RPC, in server, the RPC will pushCID into cache before serve it.
output port[1]: connect with RPC, in client, when a chunk is complete, cache will push it to RPC (higher level)
*/


class XIARouterCache : public Element { 
  public:
typedef XIAPath::handle_t handle_t;        
    XIARouterCache();
    ~XIARouterCache();
    const char *class_name() const		{ return "XIARouterCache"; }
    const char *port_count() const		{ return "2/2"; }
    const char *processing() const		{ return PUSH; }
    int configure(Vector<String> &, ErrorHandler *);         
    void push(int port, Packet *);            
  private:
    XIAPath local_addr;
    XIAXIDRouteTable *routeTable;  //XIAXIDRouteTable 
    bool cache_content_from_network;
    HashTable<XID,CChunk*> partialTable;
    HashTable<XID, CChunk*> contentTable;
    
    HashTable<XID, CChunk*> oldPartial; //used in client
    XID hid;
    
    unsigned int usedSize;
    static const unsigned int MAXSIZE=CACHESIZE;
    static const unsigned int PKTSIZE=PACKETSIZE;    
    //lru    
    static const int REFRESH=10000;
    int timer;
    HashTable<XID, int> partial;
    HashTable<XID, int> content;   
    
    int MakeSpace(int);    
    //modify routing table
    void addRoute(const XID &cid) {
//std::cout<<"addRoute"<<std::endl;
	String cmd=cid.unparse()+" 4";
	HandlerCall::call_write(routeTable, "add", cmd);
    }    
    void delRoute(const XID &cid) {
	String cmd= cid.unparse();
	HandlerCall::call_write(routeTable, "remove", cmd);
    }    
};

CLICK_ENDDECLS

#endif




