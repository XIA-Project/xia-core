#ifndef CLICK_XTRANSPORT_HH
#define CLICK_XTRANSPORT_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiaheader.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>
#include <click/xiasecurity.hh>
#include <clicknet/xia.h>
#include "xiaxidroutetable.hh"
#include <clicknet/udp.h>
#include <click/string.hh>
#include <elements/ipsec/sha1_impl.hh>
#include <click/xiatransportheader.hh>
#include <click/error.hh>
#include <click/error-syslog.hh>


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

// FIXME: put these in a std location that can be found by click and the API
#define XOPT_HLIM       0x07001
#define XOPT_NEXT_PROTO 0x07002
#define XOPT_BLOCK      0x07003
#define XOPT_ERROR_PEEK 0x07004

#ifndef DEBUG
#define DEBUG 0
#endif


#define UNUSED(x) ((void)(x))

#define ACK_DELAY			300
#define TEARDOWN_DELAY		240000
#define HLIM_DEFAULT		250
#define LAST_NODE_DEFAULT	-1
#define RANDOM_XID_FMT		"%s:30000ff0000000000000000000000000%08x"
#define UDP_HEADER_SIZE		8

#define XSOCKET_INVALID -1	// invalid socket type
#define XSOCKET_STREAM	1	// Reliable transport (SID)
#define XSOCKET_DGRAM	2	// Unreliable transport (SID)
#define XSOCKET_RAW		3	// Raw XIA socket
#define XSOCKET_CHUNK	4	// Content Chunk transport (CID)

// TODO: switch these to bytes, not packets?
#define MAX_SEND_WIN_SIZE 256  // in packets, not bytes
#define MAX_RECV_WIN_SIZE 256
#define DEFAULT_SEND_WIN_SIZE 128
#define DEFAULT_RECV_WIN_SIZE 128

#define MAX_CONNECT_TRIES	 30
#define MAX_CLOSE_TRIES 30 // added by chenren
#define MAX_MIGRATE_TRIES	 30
#define MAX_RETRANSMIT_TRIES 100

#define REQUEST_FAILED		0x00000001
#define WAITING_FOR_CHUNK	0x00000002
#define READY_TO_READ		0x00000004
#define INVALID_HASH		0x00000008

#define HASH_KEYSIZE    20

#define API_PORT    0
#define BAD_PORT       1
#define NETWORK_PORT    2
#define CACHE_PORT      3
#define XHCP_PORT       4

// chenren add
#define NOTCONNECTED 0
#define CONNECTING -1
#define CONNECTED 1

#define ACTIVE 0
#define CLOSING -1
#define CLOSED 1
// chenren add

CLICK_DECLS

/**
XTRANSPORT:
input port[0]:  api port
input port[1]:  Unused
input port[2]:  Network Rx data port
input port[3]:  in from cache

output[3]: To cache for putCID
output[2]: Network Tx data port
output[0]: Socket (API) Tx data port

Might need other things to handle chunking
*/


typedef struct {
	bool forever;
	Timestamp expiry;
	HashTable<unsigned short, unsigned int> events;
} PollEvent;

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
	XID local_4id() { return _local_4id; };
	void add_handlers();
	static int write_param(const String &, Element *, void *vparam, ErrorHandler *);

	int initialize(ErrorHandler *);
	void run_timer(Timer *timer);

	void ReturnResult(int sport, xia::XSocketMsg *xia_socket_msg, int rc = 0, int err = 0);

  private:
	SyslogErrorHandler *_errh;

	Timer _timer;

	unsigned _ackdelay_ms;
    unsigned _migrateackdelay_ms;
	unsigned _teardown_wait_ms;

	uint32_t _cid_type, _sid_type;
	XID _local_hid;
	XIAPath _local_addr;
	XID _local_4id;
	XID _null_4id;
	bool _is_dual_stack_router;
	XIAPath _nameserver_addr;

	Packet* UDPIPPrep(Packet *, int);


/* TODO: sock (previously named DAGinfo) stores per-socket states for ALL transport protocols. We better make a specialized struct for each protocol
*	(e.g., xsp_sock, tcp_sock) that is inherited from sock struct. Not sure if HashTable<XID, sock> will work. Use HashTable<XID, sock*> instead?
*/

	/* =========================
	 * Socket states
	 * ========================= */
	struct sock {
		sock(): port(0), connState(NOTCONNECTED), closeState(ACTIVE), isBlocking(true), initialized(false), // chenren: changed connState and closeState from false to 0
							full_src_dag(false), timer_on(false), synack_waiting(false),
							synackack_waiting(false), finack_waiting(false), finackack_waiting(false), // chenren: added
							so_error(0), dataack_waiting(false), teardown_waiting(false),
							send_buffer_size(DEFAULT_SEND_WIN_SIZE),
							recv_buffer_size(DEFAULT_RECV_WIN_SIZE), send_base(0),
							next_send_seqnum(0), recv_base(0), next_recv_seqnum(0),
							dgram_buffer_start(0), dgram_buffer_end(-1),
							recv_buffer_count(0), recv_pending(false), polling(0),
							did_poll(false) {};

	/* =========================
	 * Common Socket states
	 * ========================= */
		unsigned short port;
		XIAPath src_path;
		XIAPath dst_path;
		int nxt;
		int last;
		uint8_t hlim;

		bool full_src_dag; // bind to full dag or just to SID
		int sock_type; // 0: Reliable transport (SID), 1: Unreliable transport (SID), 2: Content Chunk transport (CID)

	/* =========================
	 * XSP/XChunkP Socket states
	 * ========================= */

		int connState; 			// chenren: change bool to int; {-1,0,1} represents pending, closed and connected
		int	closeState; 		// chenren: added: {-1,0,1} represents pending, connected and closed
		bool initialized;
		bool isListenSocket;
		bool isBlocking;
		bool synack_waiting;
		bool synackack_waiting; // chenren: used for synack retransmission
		bool finack_waiting;	// chenren: used for fin retransmission
		bool finackack_waiting; // chenren: used for finack retransmission
		bool dataack_waiting;
		bool teardown_waiting;
		bool migrateack_waiting;
		unsigned backlog;
		int so_error;			// used by non-blocking connect, accessed via getsockopt(SO_ERROR)
		int so_debug;			// set/read via SO_DEBUG. could be used for tracing
		int interface_id;		// port of the interface the packets arrive on
		String last_migrate_ts;

		bool did_poll;
		unsigned polling;

		int num_connect_tries; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)
		int num_migrate_tries; // number of migrate tries (Connection closes after MAX_MIGRATE_TRIES trials)
		int num_retransmit_tries; // number of times to try resending data packets
		int num_close_tries; // chenren: added for closing connections
    	queue<sock*> pending_connection_buf;
		queue<xia::XSocketMsg*> pendingAccepts; // stores accept messages from API when there are no pending connections

		// send buffer
    	WritablePacket *send_buffer[MAX_SEND_WIN_SIZE]; // packets we've sent but have not gotten an ACK for // TODO: start smaller, dynamically resize if app asks for more space (up to MAX)?
		uint32_t send_buffer_size;
    	uint32_t send_base; // the sequence # of the oldest unacked packet
    	uint32_t next_send_seqnum; // the smallest unused sequence # (i.e., the sequence # of the next packet to be sent)
		uint32_t remote_recv_window; // num additional *packets* the receiver has room to buffer

		// receive buffer
    	WritablePacket *recv_buffer[MAX_RECV_WIN_SIZE]; // packets we've received but haven't delivered to the app // TODO: start smaller, dynamically resize if app asks for more space (up to MAX)?
		uint32_t recv_buffer_size; // the number of PACKETS we can buffer (received but not delivered to app)
		uint32_t recv_base; // sequence # of the oldest received packet not delivered to app
    	uint32_t next_recv_seqnum; // the sequence # of the next in-order packet we expect to receive
		int dgram_buffer_start; // the first undelivered index in the recv buffer (DGRAM only)
		int dgram_buffer_end; // the last undelivered index in the recv buffer (DGRAM only)
		uint32_t recv_buffer_count; // the number of packets in the buffer (DGRAM only)
		bool recv_pending; // true if we should send received network data to app upon receiving it
		xia::XSocketMsg *pending_recv_msg;

		//Vector<WritablePacket*> pkt_buf;
		WritablePacket *syn_pkt;
		WritablePacket *migrate_pkt;
		WritablePacket *synack_pkt; // chenren: for retransmission
		WritablePacket *fin_pkt; 	// chenren: for retransmission
		WritablePacket *finack_pkt; // chenren: for retransmission
		HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
		HashTable<XID, Timestamp> XIDtoExpiryTime;
		HashTable<XID, bool> XIDtoTimerOn;
		HashTable<XID, int> XIDtoStatus; // Content-chunk request status... 1: waiting to be read, 0: waiting for chunk response, -1: failed
		HashTable<XID, bool> XIDtoReadReq; // Indicates whether ReadCID() is called for a specific CID
		HashTable<XID, WritablePacket*> XIDtoCIDresponsePkt;
		uint32_t seq_num;
		uint32_t ack_num;
		bool timer_on;
		Timestamp expiry; // chenren: used for syn packet retransmission
		Timestamp teardown_expiry;
		Timestamp synackack_expiry; // chenren: added
		Timestamp finack_expiry; // chenren: added
		Timestamp finackack_expiry; // chenren: added

		queue<uint32_t> rto_ests; // chenren: TODO: add RTO estimation later
    } ;


    list<int> xcmp_listeners;   // list of ports wanting xcmp notifications

    HashTable<XID, unsigned short> XIDtoPort;
    HashTable<XIDpair , unsigned short> XIDpairToPort;
    HashTable<unsigned short, sock*> portToSock;
    HashTable<XID, unsigned short> XIDtoPushPort;


    HashTable<unsigned short, bool> portToActive;
    HashTable<XIDpair , bool> XIDpairToConnectPending;

	// FIXME: can these be rolled into the sock structure?
	HashTable<unsigned short, int> nxt_xport;
    HashTable<unsigned short, int> hlim;

    HashTable<unsigned short, PollEvent> poll_events;


    atomic_uint32_t _id;
    bool _cksum;
    XIAXIDRouteTable *_routeTable;

	// modify routing table
    void addRoute(const XID &sid) {
			String cmd = sid.unparse() + " " + String(DESTINED_FOR_LOCALHOST);
        HandlerCall::call_write(_routeTable, "add", cmd);
	}

    void delRoute(const XID &sid) {
			String cmd = sid.unparse();
        HandlerCall::call_write(_routeTable, "remove", cmd);
	}



  protected:
    void copy_common(struct sock *sk, XIAHeader &xiahdr, XIAHeaderEncap &xiah);
    WritablePacket* copy_packet(Packet *, struct sock *);
    WritablePacket* copy_cid_req_packet(Packet *, struct sock *);
    WritablePacket* copy_cid_response_packet(Packet *, struct sock *);

    char *random_xid(const char *type, char *buf);

	uint32_t calc_recv_window(sock *sk);
	bool should_buffer_received_packet(WritablePacket *p, sock *sk);
	void add_packet_to_recv_buf(WritablePacket *p, sock *sk);
	void check_for_and_handle_pending_recv(sock *sk);
	int read_from_recv_buf(xia::XSocketMsg *xia_socket_msg, sock *sk);
	uint32_t next_missing_seqnum(sock *sk);
	void resize_buffer(WritablePacket* buf[], int max, int type, uint32_t old_size, uint32_t new_size, int *dgram_start, int *dgram_end);
	void resize_send_buffer(sock *sk, uint32_t new_size);
	void resize_recv_buffer(sock *sk, uint32_t new_size);

	bool usingRendezvousDAG(XIAPath bound_dag, XIAPath pkt_dag);

    void ProcessAPIPacket(WritablePacket *p_in);
    void ProcessNetworkPacket(WritablePacket *p_in);
    void ProcessCachePacket(WritablePacket *p_in);
    void ProcessXhcpPacket(WritablePacket *p_in);

    void CreatePollEvent(unsigned short _sport, xia::X_Poll_Msg *msg);
    void ProcessPollEvent(unsigned short, unsigned int);
    void CancelPollEvent(unsigned short _sport);
    /*
    ** Xsockets API handlers
    */
    void Xsocket(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xsetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xgetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xbind(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xclose(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xconnect(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xlisten(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
	void XreadyToAccept(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xaccept(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xchangead(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xreadlocalhostaddr(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xupdatenameserverdag(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xreadnameserverdag(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xgetpeername(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xgetsockname(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xisdualstackrouter(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xsend(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
    void Xsendto(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
	void Xrecv(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
	void Xrecvfrom(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void XrequestChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
    void XgetChunkStatus(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void XreadChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void XremoveChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void XpushChunkto(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
    void XbindPush(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void XputChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xpoll(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xupdaterv(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);

    void ProcessAckPacket(WritablePacket *p_in);
    void ProcessXcmpPacket(WritablePacket*p_in);
    void ProcessMigratePacket(WritablePacket *p_in);
    void ProcessMigrateAck(WritablePacket *p_in);
    void ProcessSynPacket(WritablePacket *p_in);
    void ProcessSynAckPacket(WritablePacket *p_in);
    void ProcessStreamDataPacket(WritablePacket *p_in);
    void ProcessFinPacket(WritablePacket *p_in);
    void ProcessFinAckPacket(WritablePacket *p_in);
    void ProcessDatagramPacket(WritablePacket *p_in);

    int HandleStreamRawPacket(WritablePacket *p_in);

    void RetransmitSYN(sock *sk, unsigned short _sport, Timestamp &now);

};


CLICK_ENDDECLS

#endif
