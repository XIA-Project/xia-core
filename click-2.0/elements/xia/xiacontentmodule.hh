#ifndef CLICK_XIACONTENTMODULE_HH
#define CLICK_XIACONTENTMODULE_HH
#include <click/config.h>
#include <click/packet.hh>
#include <click/xid.hh>
#include <click/list.hh>
#include <clicknet/xia.h>
#include <click/hashtable.hh>
#include <click/xiapath.hh>
#include "xiaxidroutetable.hh"
#include "xiatransport.hh"

#define CACHESIZE 1024*1024*1024    //in router 
#define CLIENTCACHE
#define PACKETSIZE 1024		

CLICK_DECLS
class XIAContentModule;
class XIATransport;

class CPart{
    public: 
	CPart(unsigned int,unsigned int);
	~CPart(){}
	unsigned int offset;
	unsigned int length;
};

struct CPartListNode {
	CPart v;
 	List_member<CPartListNode> link;
	CPartListNode(CPart p) : v(p) {}
};

typedef List<CPartListNode, &CPartListNode::link> CPartList;

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
	CPartList parts;

	void Merge(CPartList::iterator);
};

class XIAContentModule {
    friend class XIATransport;
    friend class XIACache;
    public:
    typedef XIAPath::handle_t handle_t;        
    XIAContentModule(XIATransport* transport);
    ~XIAContentModule();
    void cache_incoming(Packet *p, const XID &, const XID &, int port);
    void process_request(Packet *p, const XID &, const XID &);

    private:
    XIATransport* _transport;
    XIAPath _local_addr;
    XIAXIDRouteTable *_routeTable;  //XIAXIDRouteTable 
    bool _cache_content_from_network;
    HashTable<XID,CChunk*> _partialTable;
    HashTable<XID, CChunk*>_contentTable;
    HashTable<XID, CChunk*> oldPartial; //used in client

    unsigned int usedSize;
    static const unsigned int MAXSIZE=CACHESIZE;
    static unsigned int PKTSIZE;    
    //lru    
    static const int REFRESH=10000;
    int _timer;
    HashTable<XID, int> partial;
    HashTable<XID, int> content;   

    int MakeSpace(int);    

    //modify routing table
    void addRoute(const XID &cid) {
	String cmd=cid.unparse()+" 4";
	HandlerCall::call_write(_routeTable, "add", cmd);
    } 

    void delRoute(const XID &cid) {
	String cmd= cid.unparse();
	HandlerCall::call_write(_routeTable, "remove", cmd);
    }    
};

CLICK_ENDDECLS
#endif
