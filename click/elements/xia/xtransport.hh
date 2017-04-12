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
#include <clicknet/tcp.h>
#include <click/string.hh>
#include <click/xiaifacetable.hh>
#include <click/error.hh>
#include <click/error-syslog.hh>
#include <click/task.hh> /* Task */

#include "clicknet/tcp_timer.h"
#include "clicknet/tcp_var.h"

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
using namespace xia;

#endif

// FIXME: put these in a std location that can be found by click and the API
// XIA Specific commands for [get|set]sockopt
#define XOPT_HLIM	    0x07001
#define XOPT_NEXT_PROTO 0x07002
#define XOPT_BLOCK	    0x07003
#define XOPT_ERROR_PEEK 0x07004

// various constants
#define ACK_DELAY			300
#define MIGRATEACK_DELAY	3000
#define TEARDOWN_DELAY		240000
#define RANDOM_XID_FMT		"%s:30000ff0000000000000000000000000%08x"
#define UDP_HEADER_SIZE		8

// TODO: switch these to bytes, not packets?
#define MAX_SEND_WIN_SIZE	  256  // in packets, not bytes
#define MAX_RECV_WIN_SIZE	  256
#define DEFAULT_SEND_WIN_SIZE 128
#define DEFAULT_RECV_WIN_SIZE 128

#define MAX_RETRANSMIT_TRIES 30

#define API_PORT	 0
#define NETWORK_PORT 1

// Verbosity Bitmask definitions, directly from tcpspeaker code
#define VERB_NONE       0
#define VERB_ALL        0xffffffff
#define VERB_ERRORS     0x01
#define VERB_WARNINGS   0x02
#define VERB_INFO       0x04
#define VERB_DEBUG      0x08
#define VERB_TIMERS      0x09
#define VERB_MFD_QUEUES 0x10 // for the MFD Handler Queues
#define VERB_PACKETS    0x20 // triggered on packet handling/traversal events
#define VERB_DISPATCH   0x40 // for anything related to interconnecting handlers
#define VERB_MFH_STATE  0x80 // for the 5-state statemachine of MFH

#define INITIAL_ID 100

enum SocketState {INACTIVE = 0, LISTEN, SYN_RCVD, SYN_SENT, CONNECTED, FIN_WAIT1, FIN_WAIT2, TIME_WAIT, CLOSING, CLOSE_WAIT, LAST_ACK, CLOSED};
enum HandlerState { CREATE, INITIALIZE, ACTIVE, SHUTDOWN, CLOSE };

CLICK_DECLS

typedef struct {
	bool forever;
	Timestamp expiry;
	HashTable<uint32_t, unsigned int> events;
} PollEvent;

#ifndef TCP_GLOBALS
#define TCP_GLOBALS
struct tcp_globals
{
        int     tcp_keepidle;
        int     tcp_keepintvl;
        int     tcp_maxidle;
        int     tcp_mssdflt;
        int     tcp_rttdflt;
        int     so_flags;
        int     so_idletime;
        int     window_scale;
        bool    use_timestamp;
        uint32_t tcp_now;
        tcp_seq_t so_recv_buffer_size;
};
#endif

class sock;

class XTRANSPORT : public Element {
	friend class XDatagram;
	friend class XStream;
public:
	XTRANSPORT();
	~XTRANSPORT();

	const char *class_name() const { return "XTRANSPORT"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void push(int port, Packet *);
	int initialize(ErrorHandler *);
	void run_timer(Timer *timer);

	void add_handlers();

	static int write_param(const String &, Element *, void *vparam, ErrorHandler *);
	//ErrorHandler *error_handler()   { return _errhandler; }
    int verbosity()             { return _verbosity; }

private:
	Timer _timer;

	XIAPath _local_addr;
	String _hostname;
	XID _hid;
	XID _local_4id;
	XID _null_4id;
	XID _xcache_sid;
	XIAInterfaceTable _interfaces;
	bool _is_dual_stack_router;
	int _num_ports;
	XIAPath _nameserver_addr;
	uint32_t _next_id;

	Packet* UDPIPPrep(Packet *, int);
    bool migratable_sock(sock *, int);
    bool update_src_path(sock *, XIAPath&);
	String routeTableString(String xid);
	void remove_route(String xidstr);
	void update_route(String xid, String table, int iface, String next);
	void update_default_route(String table_str, int interface, String next_xid);
	void change_default_routes(int interface, String rhid);

	/* TCP related fields */
    tcp_globals *globals()  { return &_tcp_globals; }
    uint32_t tcp_now()      { return _tcp_globals.tcp_now; }
    // Element Handler Methods
    static String read_verb(Element*, void*);
    static int write_verb(const String&, Element*, void*, ErrorHandler*);
    tcpstat         _tcpstat;
    Timer           *_fast_ticks;
    Timer           *_slow_ticks;
    Timer           *_reaper;
    int         _verbosity;

    tcp_globals     _tcp_globals;
	ErrorHandler    *_errhandler;

	// list of ids wanting xcmp notifications
	list<uint32_t> xcmp_listeners;

	// list of ids waiting for a notification
	list <uint32_t> notify_listeners;

	// outstanding poll/selects indexed by control socket port #
	HashTable<unsigned short, PollEvent> poll_events;

	// For Content Push APIs
	HashTable<XID, unsigned short> XIDtoPushPort;

	// incoming connection to socket mapping
	HashTable<XID, sock*> XIDtoSock;
	HashTable<XIDpair , sock*> XIDpairToSock;

	// find sock structure based on API port #
	HashTable<uint32_t, sock*> idToSock;

	// servers keep track of in process connect attempts here
	HashTable<XIDpair , sock*> XIDpairToConnectPending;

	// atomic_uint32_t _id;	// FIXME: is this a click thing, or can we delete it?

	/* =========================
	 * Xtransport Methods
	* ========================= */
	XID local_hid()	  { return _hid; };
	XIAPath local_addr() { return _local_addr; };
	XID local_4id()	  { return _local_4id; };
	void ReturnResult(unsigned short sport, xia::XSocketMsg *xia_socket_msg, int rc = 0, int err = 0);

	char *random_xid(const char *type, char *buf);

	bool usingRendezvousDAG(XIAPath bound_dag, XIAPath pkt_dag);

	void ProcessAPIPacket(WritablePacket *p_in);
	void ProcessNetworkPacket(WritablePacket *p_in);
	void ProcessXhcpPacket(WritablePacket *p_in);

	void ProcessPollEvent(uint32_t id, unsigned int);
	void CreatePollEvent(unsigned short _sport, xia::X_Poll_Msg *msg);
	void CancelPollEvent(unsigned short _sport);
	/*
	** Xsockets API handlers
	*/
	void Xsocket(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xsetsockopt(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xgetsockopt(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xbind(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xclose(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xconnect(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xlisten(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void XreadyToAccept(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xaccept(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xupdatedag(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xreadlocalhostaddr(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void XsetXcacheSid(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xupdatenameserverdag(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xgethostname(unsigned short _sport, uint32_t id,  xia::XSocketMsg *xia_socket_msg);
	void Xgetifaddrs(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xreadnameserverdag(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xgetpeername(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xgetsockname(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xisdualstackrouter(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xsend(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
	void Xsendto(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in);
	void Xrecv(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xrecvfrom(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xpoll(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xupdaterv(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xfork(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xreplay(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xnotify(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void XmanageFID(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xupdatedefiface(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);
	void Xdefaultiface(unsigned short _sport, uint32_t id, xia::XSocketMsg *xia_socket_msg);

	// protocol handlers
	void ProcessDatagramPacket(WritablePacket *p_in);
	void ProcessStreamPacket(WritablePacket *p_in);
	int HandleStreamRawPacket(WritablePacket *p_in);

	// socket teardown
	bool TeardownSocket(sock *sk);

	void ProcessXcmpPacket(WritablePacket*p_in);

	void ScheduleTimer(sock *sk, int delay);
	void CancelRetransmit(sock *sk);
	sock *XID2Sock(XID dest_xid);

	static const char *StateStr(SocketState state);
	static const char *SocketTypeStr(int);
	void ChangeState(sock *sk, SocketState state);

	static String Netstat(Element *e, void *thunk);
	static String default_addr(Element *e, void *thunk);
	static String ns_addr(Element *e, void *thunk);
	static String rv_addr(Element *e, void *thunk);
	static String rv_control_addr(Element *e, void *thunk);
	static int purge(const String &conf, Element *e, void *thunk, ErrorHandler *errh);
	int IfaceFromSIDPath(XIAPath sidPath);
	void _add_ifaddr(xia::X_GetIfAddrs_Msg *_msg, int interface);

	// modify routing table
	void manageRoute(const XID &xid, bool create);
	void addRoute(const XID &xid) {
		manageRoute(xid, true);
	}
	void delRoute(const XID &xid) {
		manageRoute(xid, false);
	}

	// mobility
	void migrateActiveSessions(int interface, XIAPath new_dag);

	uint32_t NewID();

	uint32_t NextFIDSeqNo(sock *sk, XIAPath &dst);
	bool run_task(Task*);

};
 typedef HashTable<XIDpair, sock*>::iterator ConnIterator;

/* =========================
	 * Socket states
	 * ========================= */
class sock : public Element {
	friend class XTRANSPORT;
	public:
		const char *class_name() const      { return "sock"; }

    virtual bool run_task(Task*) { return false; };

    using Element::push;
    void push(WritablePacket *){};
    // virtual Packet *pull(const int port) = 0;
    int read_from_recv_buf(XSocketMsg *xia_socket_msg) ;
    // virtual ~XGenericTransport();
    unsigned short get_port() {return port;}
    int get_type() { return sock_type; }
    void set_state(const HandlerState s) {hstate = s;}
    HandlerState get_state() { return hstate; }
    XIAPath get_src_path() {return src_path;}
    void set_src_path(XIAPath p) {src_path = p;}
    XIAPath get_dst_path() {return dst_path;}
    void set_dst_path(XIAPath p) {dst_path = p;}
    uint8_t get_hlim() {return hlim;}
    void set_hlim(uint8_t n) {hlim = n;}
    uint8_t get_hop_count() {return hop_count;}
    void set_hop_count(uint8_t n) {hop_count = n;}
    bool is_full_src_dag() {return full_src_dag;}
    void set_full_src_dag(bool f) {full_src_dag = f;}
    String get_sdag() {return sdag;}
    void set_sdag(String s) {sdag = s;}
    String get_ddag() {return ddag;}
    void set_ddag(String s) {ddag = s;}
    bool is_did_poll() {return did_poll;}
    void set_did_poll(bool d) {did_poll = d;}
    unsigned get_polling() {return polling;}
    void increase_polling() {polling++;}
    void decrease_polling() {polling--;}
    bool is_recv_pending() {return recv_pending;}
    void set_recv_pending(bool r) {recv_pending = r;}
    void set_pending_recv_msg(XSocketMsg *msg) {pending_recv_msg = msg;}
    XIDpair get_key() {return key;}
    void set_key(XIDpair k) {key = k;}
	uint32_t get_id() { return id; }
	void set_id(uint32_t new_id) { id = new_id; }

    XTRANSPORT *get_transport() { return transport; }
	sock(XTRANSPORT *transport, unsigned short port, uint32_t id, int type);
	sock();
	/* =========================
	 * Common Socket states
	 * ========================= */
	unsigned short port;		// API Port
	int sock_type;				// STREAM, DGRAM, RAW, CHUNK
	SocketState state;			// Socket state (Mainly for STREAM)
	bool isBlocking;			// true if socket is blocking (default)
	bool initialized;			// FIXME: used by dgram and chunks. can we replace it?
	int so_error;				// used by non-blocking connect, accessed via getsockopt(SO_ERROR)
	int so_debug;				// set/read via SO_DEBUG. could be used for tracing in the future
	int interface_id;			// port of the interface the packets arrive on
	int outgoing_iface;			// interface matching src path (if any)
	unsigned polling;			// # of outstanding poll/select requests on this socket
	bool recv_pending;			// true if API is waiting to receive data
	bool timer_on;				// if true timer is enabled
	Timestamp expiry;			// when timer should fire next

	XIAPath src_path;			// peer DAG
	XIAPath dst_path;			// our DAG
	uint8_t hlim;				// hlim/ttl
	uint8_t hop_count;

	bool full_src_dag;			// bind to full dag or just to SID

	unsigned short nxt_xport;

	int refcount;				// # of processes that have this socket open

	/* =========================
	 * Flooding state
	 * ========================= */
	 HashTable<XIDpair, uint32_t> flood_sequence_numbers;

	/* =========================
	 * "TCP" state
	 * ========================= */
	unsigned backlog;			// max # of outstanding connections
	uint32_t seq_num;
	uint32_t ack_num;
	bool isAcceptedSocket;		// true if this socket is generated due to an accept
	int num_connect_tries;		// FIXME: can these all be flattened into one variable?
	int num_retransmits;
	int num_close_tries;
	WritablePacket *pkt;		// Control packet waiting to be ack'd (FIXME: could this just go in the send buffer?)

	// connect/accept
	queue<sock*> pending_connection_buf;	// list of outstanding connections waiting to be accepted
	queue<xia::XSocketMsg*> pendingAccepts;	// stores accept messages from API when there are no pending connections

	// send buffer
	uint32_t send_buffer_size;
	uint32_t send_base;				// the sequence # of the oldest unacked packet
	uint32_t next_send_seqnum;		// the smallest unused sequence # (i.e., the sequence # of the next packet to be sent)
	uint32_t remote_recv_window;	// num additional *packets* the receiver has room to buffer
	WritablePacket *send_buffer[MAX_SEND_WIN_SIZE]; // packets we've sent but have not gotten an ACK for

	/* =========================
	 * shared tcp/udp receive buffers
	 * ========================= */
	WritablePacket *recv_buffer[MAX_RECV_WIN_SIZE]; // packets we've received but haven't delivered to the app
	uint32_t recv_buffer_size;		// the number of PACKETS we can buffer (received but not delivered to app)
	uint32_t recv_base;				// sequence # of the oldest received packet not delivered to app
	uint32_t next_recv_seqnum;		// the sequence # of the next in-order packet we expect to receive
	int dgram_buffer_start;			// the first undelivered index in the recv buffer (DGRAM only)
	int dgram_buffer_end;			// the last undelivered index in the recv buffer (DGRAM only)
	uint32_t recv_buffer_count;		// the number of packets in the buffer (DGRAM only)
	xia::XSocketMsg *pending_recv_msg;

	/* =========================
	 * tcp connection migration
	 * ========================= */
	bool migrating;
	bool migrateacking;
	String last_migrate_ts;			// timestamp of last migrate/migrateack seen

	/*
	 * =========================
	 * rendezvous state
	 * ========================= */
	bool rv_modified_dag;

	/* =========================
	 * Chunk States
	* ========================= */
	bool xcacheSock;

	bool reap;
protected:
    XTRANSPORT *transport;
    HandlerState hstate;
    XIDpair key;

	uint32_t id;

    String sdag;
    String ddag;

    bool did_poll;
    ErrorHandler    *_errh;
	} ;


CLICK_ENDDECLS
#endif
