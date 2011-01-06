#ifndef CLICK_XIAROUTERCACHE_HH
#define CLICK_XIAROUTERCACHE_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/hashtable.hh>
//#include <map>
#include <list>
#include <string.h>

#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>

#define CACHESIZE 1024*1024*1024    //in router 
//#define CLIENTCACHE
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
    HashTable<XID,CChunk*> partialTable;
    HashTable<XID, CChunk*> contentTable;
    
    HashTable<XID, CChunk*> oldPartial;
    
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
	String cmd=cid.unparse()+" 4";
	HandlerCall::call_write(routeTable, "add", cmd);
    }    
    void delRoute(const XID &cid) {
	String cmd= cid.unparse();
	HandlerCall::call_write(routeTable, "del", cmd);
    }    
};


CLICK_ENDDECLS
#endif




