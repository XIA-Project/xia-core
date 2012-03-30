#ifndef CLICK_XIACONTENTMODULE_HH
#define CLICK_XIACONTENTMODULE_HH
#include <click/config.h>
#include <click/packet.hh>
#include <click/xid.hh>
#include <click/list.hh>
#include <clicknet/xia.h>
#include <click/hashtable.hh>
#include <click/xiapath.hh>
#include <map>

#include "xiaxidroutetable.hh"
#include "xiatransport.hh"

#define CACHESIZE 1024*1024*1024    //only for router cache (endhost cahe is virtually unlimited, but is periodically refreshed)
#define CLIENTCACHE
#define PACKETSIZE 1024		

#define POLICY_FIFO 1

#define HASH_KEYSIZE 20

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
	XID id() { return xid; };
    private:
	XID xid;
	bool complete;
	unsigned int size;
	char* payload;
	CPartList parts; 
	bool deleted;

	void Merge(CPartList::iterator);
};

/* Client local cache*/
struct contentMeta{
    int ttl;
    struct timeval timestamp;
    int chunkSize;
};

struct cacheMeta{
    int curSize;
    int maxSize;
    int policy;
    HashTable <XID, struct contentMeta*> *contentMetaTable;
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

    protected:
    void cache_incoming_local(Packet *p, const XID& srcCID, bool local_putcid);
    void cache_incoming_forward(Packet *p, const XID& srcCID);
    void cache_incoming_remove(Packet *p, const XID& srcCID);
    void cache_management();
    private:
    XIATransport* _transport;
    XIAPath _local_addr;
    XIAXIDRouteTable *_routeTable;  //XIAXIDRouteTable 
    bool _cache_content_from_network;
    HashTable<XID,CChunk*> _partialTable;
    HashTable<XID, CChunk*>_contentTable;
    HashTable<XID, CChunk*>_oldPartial; /* only used in client. When refresh timer is
	fired,  _oldPartial is cleared and everything in _partialTable goes to _oldPartial.
	It takes two timers to clear on-going partial chunk transfers.  */

    HashTable<int, cacheMeta*> _cacheMetaTable;
    
    unsigned int usedSize;
    static const unsigned int MAXSIZE=CACHESIZE;
    static unsigned int PKTSIZE;    
    //lru    
    static const int REFRESH=1000000;
    int _timer;
    HashTable<XID, int> partial;
    HashTable<XID, int> content;   
    Packet *makeChunkResponse(CChunk * chunk, Packet *p_in);
    int MakeSpace(int);    

    //Cache Policy
    void applyLocalCachePolicy(int);
    const XID* findOldestContent(int);
    bool isExpiredContent(struct contentMeta* cm);

    //modify routing table
    void addRoute(const XID &cid) {
	String cmd=cid.unparse()+" 4";
	click_chatter("Add route %s", cid.unparse().c_str());
	HandlerCall::call_write(_routeTable, "add", cmd);
    } 

    void delRoute(const XID &cid) {
	String cmd= cid.unparse();
	click_chatter("Del route %s", cid.unparse().c_str());
	HandlerCall::call_write(_routeTable, "remove", cmd);
    }    
};

CLICK_ENDDECLS
#endif
