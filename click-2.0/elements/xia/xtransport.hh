#ifndef CLICK_XTRANSPORT_HH
#define CLICK_XTRANSPORT_HH

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
#include <elements/ipsec/sha1_impl.hh>
#if CLICK_USERLEVEL
#include <list>
#include <stdio.h>
#include <iostream>
#include <click/xidpair.hh>
#include <click/timer.hh>
#include <click/packet.hh>
#include <queue>

#include "../../userlevel/xia.pb.h"

using namespace std;



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

#define XSOCKET_INVALID -1 // invalid socket type	
#define XSOCKET_STREAM 1	// Reliable transport (SID)
#define XSOCKET_DGRAM 2	// Unreliable transport (SID)
#define XSOCKET_RAW	3	// Raw XIA socket
#define XSOCKET_CHUNK 4	// Content Chunk transport (CID)

#define MAX_WIN_SIZE 100

#define MAX_CONNECT_TRIES 30

#define WAITING_FOR_CHUNK 0
#define READY_TO_READ 1
#define REQUEST_FAILED -1
#define INVALID_HASH -2

#define HASH_KEYSIZE 20

CLICK_DECLS

/**
XTRANSPORT:   
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

class XTRANSPORT : public Element { 
  public:
    XTRANSPORT();
    ~XTRANSPORT();
    const char *class_name() const		{ return "XTRANSPORT"; }
    const char *port_count() const		{ return "5/4"; }
    const char *processing() const		{ return PUSH; }
    int configure(Vector<String> &, ErrorHandler *);         
    void push(int port, Packet *);            
    XID local_hid() { return _local_hid; };
    XIAPath local_addr() { return _local_addr; };
    void add_handlers();
    static int write_param(const String &, Element *, void *vparam, ErrorHandler *);
    
    int initialize(ErrorHandler *);
    void run_timer(Timer *timer);
    
    WritablePacket* copy_packet(Packet *);
    WritablePacket* copy_cid_req_packet(Packet *);
    WritablePacket* copy_cid_response_packet(Packet *);

	void ReturnResult(int sport, xia::XSocketCallType type, int rc = 0, int err = 0);
    
  private:
    Timer _timer;
    
    unsigned _ackdelay_ms;
    unsigned _teardown_wait_ms;
    
    uint32_t _cid_type, _sid_type;
    XID _local_hid;
    XIAPath _local_addr;
    bool isConnected;
    XIAPath _nameserver_addr;

    // protobuf message
    xia::XSocketMsg xia_socket_msg;
    //enum xia_socket_msg::XSocketMsgType type;
    
    Packet* UDPIPEncap(Packet *, int,int);
    
    struct DAGinfo{
    DAGinfo(): port(0), isConnected(false), initialized(false), full_src_dag(false), timer_on(false), synack_waiting(false), dataack_waiting(false), teardown_waiting(false) {};
    unsigned short port;
    XID xid;
    XIAPath src_path;
    XIAPath dst_path;
    int nxt;
    int last;
    uint8_t hlim;
    bool isConnected;
    bool initialized;
    bool full_src_dag; // bind to full dag or just to SID  
    int sock_type; // 0: Reliable transport (SID), 1: Unreliable transport (SID), 2: Content Chunk transport (CID)
    String sdag;
    String ddag;
    int num_connect_tries; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)
    //Vector<WritablePacket*> pkt_buf;
    WritablePacket *syn_pkt;
    WritablePacket *sent_pkt[MAX_WIN_SIZE];
    HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
    HashTable<XID, Timestamp> XIDtoExpiryTime;
    HashTable<XID, bool> XIDtoTimerOn;
    HashTable<XID, int> XIDtoStatus; // Content-chunk request status... 1: waiting to be read, 0: waiting for chunk response, -1: failed
    HashTable<XID, bool> XIDtoReadReq; // Indicates whether ReadCID() is called for a specific CID
    HashTable<XID, WritablePacket*> XIDtoCIDresponsePkt;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t base; // the sequence # of the oldest unacked packet
    uint32_t next_seqnum; // the smallest unused sequence # (i.e., the sequence # of the next packet to be sent)
    uint32_t expected_seqnum; // the sequence # of the next in-order packet (this is used at receiver-side)
    bool timer_on;
    Timestamp expiry;
    bool synack_waiting;
    bool dataack_waiting;
    bool teardown_waiting;
    Timestamp teardown_expiry;
    } ;
    
 
    HashTable<XID, unsigned short> XIDtoPort;
    HashTable<XIDpair , unsigned short> XIDpairToPort;
    HashTable<unsigned short, DAGinfo> portToDAGinfo;
    HashTable<unsigned short, int> portRxSeqNo;
    HashTable<unsigned short, int> portTxSeqNo;
    HashTable<unsigned short, int> portAckNo;
    HashTable<unsigned short, bool> portToActive;
    HashTable<XIDpair , bool> XIDpairToConnectPending;
	HashTable<unsigned short, int> nxt_xport;
	list<int> xcmp_listeners;
    HashTable<unsigned short, int> hlim;
    queue<DAGinfo> pending_connection_buf;
    
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




