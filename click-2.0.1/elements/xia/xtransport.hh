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
// XIA Specific commands for [get|set]sockopt
#define XOPT_HLIM	    0x07001
#define XOPT_NEXT_PROTO 0x07002
#define XOPT_BLOCK	    0x07003
#define XOPT_ERROR_PEEK 0x07004

// various constants
#define ACK_DELAY			300
#define MIGRATEACK_DELAY	3000
#define TEARDOWN_DELAY		120000
#define HLIM_DEFAULT		250
#define LAST_NODE_DEFAULT	-1
#define RANDOM_XID_FMT		"%s:30000ff0000000000000000000000000%08x"
#define UDP_HEADER_SIZE		8

// SOCK_STREAM, etc from std socket definitions
#define SOCK_CHUNK		4

// TODO: switch these to bytes, not packets?
#define MAX_SEND_WIN_SIZE	  256  // in packets, not bytes
#define MAX_RECV_WIN_SIZE	  256
#define DEFAULT_SEND_WIN_SIZE 128
#define DEFAULT_RECV_WIN_SIZE 128

#define MAX_RETRANSMIT_TRIES 30

#define REQUEST_FAILED	  0x00000001
#define WAITING_FOR_CHUNK 0x00000002
#define READY_TO_READ	  0x00000004
#define INVALID_HASH	  0x00000008

#define HASH_KEYSIZE 20

#define API_PORT	 0
#define BAD_PORT	 1  // FIXME why do we still have this?
#define NETWORK_PORT 2
#define CACHE_PORT   3
#define XHCP_PORT	 4

enum SocketState {INACTIVE = 0, LISTEN, SYN_RCVD, SYN_SENT, CONNECTED, FIN_WAIT1, FIN_WAIT2, TIME_WAIT, CLOSING, CLOSE_WAIT, LAST_ACK, CLOSED};

CLICK_DECLS

typedef struct {
	bool forever;
	Timestamp expiry;
	HashTable<unsigned short, unsigned int> events;
} PollEvent;


class XTRANSPORT : public Element {
public:
	XTRANSPORT();
	~XTRANSPORT();

	const char *class_name() const { return "XTRANSPORT"; }
	const char *port_count() const { return "5/4"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	void push(int port, Packet *);
	int initialize(ErrorHandler *);
	void run_timer(Timer *timer);

	XID local_hid()	  { return _local_hid; };
	XIAPath local_addr() { return _local_addr; };
	XID local_4id()	  { return _local_4id; };
	void add_handlers();

	static int write_param(const String &, Element *, void *vparam, ErrorHandler *);


private:
	SyslogErrorHandler *_errh;

	Timer _timer;

	uint32_t _cid_type, _sid_type;
	XID _local_hid;
	XIAPath _local_addr;
	XID _local_4id;
	XID _null_4id;
	bool _is_dual_stack_router;
	XIAPath _nameserver_addr;

	Packet* UDPIPPrep(Packet *, int);


	/* =========================
	 * Socket states
	 * ========================= */
	struct sock {
		sock() {
			port = 0;
			sock_type = 0;
			state = INACTIVE;
			isBlocking = true;
			initialized = false;
			so_error = 0;
			so_debug = false;
			interface_id = -1;
			polling = false;
			recv_pending = false;
			timer_on = false;
			hlim = HLIM_DEFAULT;
			full_src_dag = false;
			nxt_xport = CLICK_XIA_NXT_TRN;
			backlog = 5;
			seq_num = 0;
			ack_num = 0;
			isAcceptedSocket = false;

			num_connect_tries = 0;
			num_retransmits = 0;
			num_close_tries = 0;
			
			pkt = NULL;
			send_buffer_size = DEFAULT_RECV_WIN_SIZE;
			send_base = 0;
			next_send_seqnum = 0;
			remote_recv_window = 0;
			recv_buffer_size = DEFAULT_RECV_WIN_SIZE;
			recv_base = 0;
			next_recv_seqnum = 0;
			dgram_buffer_start = 0;
			dgram_buffer_end = -1;
			recv_buffer_count = 0;
			pending_recv_msg = NULL;
			migrateack_waiting = false;
			last_migrate_ts = 0;
			num_migrate_tries = 0;
			migrate_pkt = NULL;
			recv_pending = false;
		}

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
		unsigned polling;			// # of outstanding poll/select requests on this socket
		bool recv_pending;			// true if API is waiting to receive data
		bool timer_on;				// if true timer is enabled
		Timestamp expiry;			// when timer should fire next

		XIAPath src_path;			// peer DAG
		XIAPath dst_path;			// our DAG
		uint8_t hlim;				// hlim/ttl

		bool full_src_dag;			// bind to full dag or just to SID

		unsigned short nxt_xport;

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
		bool migrateack_waiting;
		String last_migrate_ts;
		int num_migrate_tries;			// number of migrate tries (Connection closes after MAX_MIGRATE_TRIES trials)
		WritablePacket *migrate_pkt;

		/* =========================
		 * Chunk States
		* ========================= */
		HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
		HashTable<XID, Timestamp> XIDtoExpiryTime;
		HashTable<XID, bool> XIDtoTimerOn;
		HashTable<XID, int> XIDtoStatus;	// Content-chunk request status... 1: waiting to be read, 0: waiting for chunk response, -1: failed
		HashTable<XID, bool> XIDtoReadReq;	// Indicates whether ReadCID() is called for a specific CID
		HashTable<XID, WritablePacket*> XIDtoCIDresponsePkt;
	} ;

protected:
	XIAXIDRouteTable *_routeTable;

	// list of ports wanting xcmp notifications
	list<int> xcmp_listeners;

	// outstanding poll/selects indexed by API port #
	HashTable<unsigned short, PollEvent> poll_events;

	// For Content Push APIs
	HashTable<XID, unsigned short> XIDtoPushPort;

	// incoming connection to socket mapping
	HashTable<XID, sock*> XIDtoSock;
	HashTable<XIDpair , sock*> XIDpairToSock;

	// find sock structure based on API port #
	HashTable<unsigned short, sock*> portToSock;

	// servers keep track of in process connect attempts here
	HashTable<XIDpair , sock*> XIDpairToConnectPending;

	// atomic_uint32_t _id;	// FIXME: is this a click thing, or can we delete it?

	/* =========================
	 * Xtransport Methods
	* ========================= */
	void ReturnResult(int sport, xia::XSocketMsg *xia_socket_msg, int rc = 0, int err = 0);

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

	// protocol handlers
	void ProcessDatagramPacket(WritablePacket *p_in);
	void ProcessStreamPacket(WritablePacket *p_in);
	int HandleStreamRawPacket(WritablePacket *p_in);

	// socket teardown
	bool TeardownSocket(sock *sk);

	// TCP state handlers
	void ProcessAckPacket(WritablePacket *p_in);
	void ProcessXcmpPacket(WritablePacket*p_in);
	void ProcessMigratePacket(WritablePacket *p_in);
	void ProcessMigrateAck(WritablePacket *p_in);
	void ProcessSynPacket(WritablePacket *p_in);
	void ProcessSynAckPacket(WritablePacket *p_in);
	void ProcessStreamDataPacket(WritablePacket *p_in);
	void ProcessFinPacket(WritablePacket *p_in);
	void ProcessFinAckPacket(WritablePacket *p_in);

	// timer retransmit handlers
	bool RetransmitCIDRequest(sock *sk, unsigned short _sport, Timestamp &now, Timestamp &erlist_pending_expiry);
	bool RetransmitDATA(sock *sk, unsigned short _sport, Timestamp &now);
	bool RetransmitFIN(sock *sk, unsigned short _sport, Timestamp &now);
	bool RetransmitFINACK(sock *sk, unsigned short _sport, Timestamp &now);
	bool RetransmitMIGRATE(sock *sk, unsigned short _sport, Timestamp &now);
	bool RetransmitSYN(sock *sk, unsigned short _sport, Timestamp &now);
	bool RetransmitSYNACK(sock *sk, unsigned short _sport, Timestamp &now);

	void SendControlPacket(int type, sock *sk, const void *, size_t plen, XIAPath &src_path, XIAPath &dst_path);
	void MigrateFailure(sock *sk);
	void ScheduleTimer(sock *sk, int delay);
	void CancelRetransmit(sock *sk);

	static const char *StateStr(SocketState state);
	static const char *SocketTypeStr(int);
	void ChangeState(sock *sk, SocketState state);

	static String Netstat(Element *e, void *thunk);
	static int purge(const String &conf, Element *e, void *thunk, ErrorHandler *errh);

	// modify routing table
	void addRoute(const XID &sid) {
		String cmd = sid.unparse() + " " + String(DESTINED_FOR_LOCALHOST);
		HandlerCall::call_write(_routeTable, "add", cmd);
	}

	void delRoute(const XID &sid) {
		String cmd = sid.unparse();
		HandlerCall::call_write(_routeTable, "remove", cmd);
	}
};

CLICK_ENDDECLS
#endif
