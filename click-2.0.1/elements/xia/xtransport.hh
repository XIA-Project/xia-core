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
#define XOPT_HLIM 0x07001
#define XOPT_NEXT_PROTO 0x07002

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


/* TCP params from http://lxr.linux.no/linux+v3.10.2/include/net/tcp.h */

/* Offer an initial receive window of 10 mss. */
#define TCP_DEFAULT_INIT_RCVWND	10

/* Minimal accepted MSS. It is (60+60+8) - (20+20). */
#define TCP_MIN_MSS		88U

/* The least MTU to use for probing */
#define TCP_BASE_MSS		512

/* After receiving this amount of duplicate ACKs fast retransmit starts. */
#define TCP_FASTRETRANS_THRESH 3

/* Maximal reordering. */
#define TCP_MAX_REORDERING	127

/* Maximal number of ACKs sent quickly to accelerate slow-start. */
#define TCP_MAX_QUICKACKS	16U


#define TCP_RETR1	3	/*
				 * This is how many retries it does before it
				 * tries to figure out if the gateway is
				 * down. Minimal RFC value is 3; it corresponds
				 * to ~3sec-8min depending on RTO.
				 */

#define TCP_RETR2	15	/*
				 * This should take at least
				 * 90 minutes to time out.
				 * RFC1122 says that the limit is 100 sec.
				 * 15 is ~13-30min depending on RTO.
				 */

#define TCP_SYN_RETRIES	 6	/* This is how many retries are done
				 * when active opening a connection.
				 * RFC1122 says the minimum retry MUST
				 * be at least 180secs.  Nevertheless
				 * this value is corresponding to
				 * 63secs of retransmission with the
				 * current initial RTO.
				 */

#define TCP_SYNACK_RETRIES 5	/* This is how may retries are done
				 * when passive opening a connection.
				 * This is corresponding to 31secs of
				 * retransmission with the current
				 * initial RTO.
				 */

#define TCP_TIMEWAIT_LEN (60*HZ) /* how long to wait to destroy TIME-WAIT
				  * state, about 60 seconds	*/
#define TCP_FIN_TIMEOUT	TCP_TIMEWAIT_LEN
                                 /* BSD style FIN_WAIT2 deadlock breaker.
				  * It used to be 3min, new value is 60sec,
				  * to combine FIN-WAIT-2 timeout with
				  * TIME-WAIT timer.
				  */

#define TCP_DELACK_MAX	((unsigned)(HZ/5))	/* maximal time to delay before sending an ACK */
#if HZ >= 100
#define TCP_DELACK_MIN	((unsigned)(HZ/25))	/* minimal time to delay before sending an ACK */
#define TCP_ATO_MIN	((unsigned)(HZ/25))
#else
#define TCP_DELACK_MIN	4U
#define TCP_ATO_MIN	4U
#endif
#define TCP_RTO_MAX	((unsigned)(120*HZ))
#define TCP_RTO_MIN	((unsigned)(HZ/5))
#define TCP_TIMEOUT_INIT ((unsigned)(1*HZ))	/* RFC6298 2.1 initial RTO value	*/
#define TCP_TIMEOUT_FALLBACK ((unsigned)(3*HZ))	/* RFC 1122 initial RTO value, now
						 * used as a fallback RTO for the
						 * initial data transmission if no
						 * valid RTT sample has been acquired,
						 * most likely due to retrans in 3WHS.
						 */

#define TCP_RESOURCE_PROBE_INTERVAL ((unsigned)(HZ/2U)) /* Maximal interval between probes
					                 * for local resources.
					                 */

#define TCP_KEEPALIVE_TIME	(120*60*HZ)	/* two hours */
#define TCP_KEEPALIVE_PROBES	9		/* Max of 9 keepalive probes	*/
#define TCP_KEEPALIVE_INTVL	(75*HZ)

#define MAX_TCP_KEEPIDLE	32767
#define MAX_TCP_KEEPINTVL	32767
#define MAX_TCP_KEEPCNT		127
#define MAX_TCP_SYNCNT		127

#define TCP_SYNQ_INTERVAL	(HZ/5)	/* Period of SYNACK timer */

#define TCP_PAWS_24DAYS	(60 * 60 * 24 * 24)
#define TCP_PAWS_MSL	60		/* Per-host timestamps are invalidated
					 * after this time. It should be equal
					 * (or greater than) TCP_TIMEWAIT_LEN
					 * to provide reliability equal to one
					 * provided by timewait state.
					 */
#define TCP_PAWS_WINDOW	1		/* Replay window for per-host
					 * timestamps. It must be less than
					 * minimal timewait lifetime.
					 */
/*
 *	TCP option
 */
 
#define TCPOPT_NOP		1	/* Padding */
#define TCPOPT_EOL		0	/* End of options */
#define TCPOPT_MSS		2	/* Segment size negotiating */


/*
 *     TCP option lengths
 */
#define TCPOLEN_MSS            4


/* But this is what stacks really send out. */
#define TCPOLEN_MSS_ALIGNED		4

/* Flags in tp->nonagle */
#define TCP_NAGLE_OFF		1	/* Nagle's algo is disabled */
#define TCP_NAGLE_CORK		2	/* Socket is corked	    */
#define TCP_NAGLE_PUSH		4	/* Cork is overridden for already queued data */

/* TCP thin-stream limits */
#define TCP_THIN_LINEAR_RETRIES 6       /* After 6 linear retries, do exp. backoff */

/* TCP initial congestion window as per draft-hkchu-tcpm-initcwnd-01 */
#define TCP_INIT_CWND		10

#define TCP_INFINITE_SSTHRESH 0x7fffffff

//  Definitions for the TCP protocol sk_state field.
enum {
	TCP_ESTABLISHED = 0,
	TCP_SYN_SENT,
	TCP_SYN_RECV,
	TCP_FIN_WAIT1,
	TCP_FIN_WAIT2,
	TCP_TIME_WAIT,
	TCP_CLOSE,
	TCP_CLOSE_WAIT,
	TCP_LAST_ACK,
	TCP_LISTEN,
	TCP_CLOSING,	/* Now a valid state */

	TCP_NSTATES	/* Leave at the end! */
};

/* 
 * (BSD)
 * Flags used when sending segments in tcp_output.  Basic flags (TH_RST,
 * TH_ACK,TH_SYN,TH_FIN) are totally determined by state, with the proviso
 * that TH_FIN is sent only if all data queued for output is included in the
 * segment. See definition of flags in xiatransportheader.hh
 */
//static const uint8_t	tcp_outflags[TCP_NSTATES] = {
//		TH_RST|TH_ACK,		/* 0, CLOSED */
//		0,			/* 1, LISTEN */
//		TH_SYN,			/* 2, SYN_SENT */
//		TH_SYN|TH_ACK,		/* 3, SYN_RECEIVED */
//		TH_ACK,			/* 4, ESTABLISHED */
//		TH_ACK,			/* 5, CLOSE_WAIT */
//		TH_FIN|TH_ACK,		/* 6, FIN_WAIT_1 */
//		TH_FIN|TH_ACK,		/* 7, CLOSING */
//		TH_FIN|TH_ACK,		/* 8, LAST_ACK */
//		TH_ACK,			/* 9, FIN_WAIT_2 */
//		TH_ACK,			/* 10, TIME_WAIT */
//	};	

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


//  pthread_mutex_t _lock;
//  pthread_mutexattr_t _lock_attr;

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
    bool isConnected;
    XIAPath _nameserver_addr;

    Packet* UDPIPPrep(Packet *, int);


/* TODO: sock (previously named DAGinfo) stores per-socket states for ALL transport protocols. We better make a specialized struct for each protocol
*	(e.g., xsp_sock, tcp_sock) that is inherited from sock struct. Not sure if HashTable<XID, sock> will work. Use HashTable<XID, sock*> instead?
*/

	/* =========================
	 * Socket states
	 * ========================= */
    struct sock {
		sock(): port(0), isConnected(false), initialized(false), full_src_dag(false), timer_on(false), synack_waiting(false), dataack_waiting(false), teardown_waiting(false), migrateack_waiting(false), send_buffer_size(DEFAULT_SEND_WIN_SIZE), recv_buffer_size(DEFAULT_RECV_WIN_SIZE), send_base(0), next_send_seqnum(0), recv_base(0), next_recv_seqnum(0), dgram_buffer_start(0), dgram_buffer_end(-1), recv_buffer_count(0), recv_pending(false), polling(0), did_poll(false) {};

	/* =========================
	 * Common Socket states
	 * ========================= */
		unsigned short port;
		XIAPath src_path;
		XIAPath dst_path;
		int nxt;
		int last;
		uint8_t hlim;

		unsigned char sk_state;		// e.g. TCP connection state for tcp_sock

		bool full_src_dag; // bind to full dag or just to SID  
		int sock_type; // 0: Reliable transport (SID), 1: Unreliable transport (SID), 2: Content Chunk transport (CID)

	/* =========================
	 * XSP/XChunkP Socket states
	 * ========================= */

		bool isConnected;
		bool initialized;
		bool isAcceptSocket;
		bool synack_waiting;
		bool dataack_waiting;
		bool teardown_waiting;
		bool migrateack_waiting;
		String last_migrate_ts;

		bool did_poll;
		unsigned polling;

		int num_connect_tries; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)
		int num_migrate_tries; // number of migrate tries (Connection closes after MAX_MIGRATE_TRIES trials)
		int num_retransmit_tries; // number of times to try resending data packets

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
		HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
		HashTable<XID, Timestamp> XIDtoExpiryTime;
		HashTable<XID, bool> XIDtoTimerOn;
		HashTable<XID, int> XIDtoStatus; // Content-chunk request status... 1: waiting to be read, 0: waiting for chunk response, -1: failed
		HashTable<XID, bool> XIDtoReadReq; // Indicates whether ReadCID() is called for a specific CID
		HashTable<XID, WritablePacket*> XIDtoCIDresponsePkt;
		uint32_t seq_num;
		uint32_t ack_num;
		bool timer_on;
		Timestamp expiry;
		Timestamp teardown_expiry;

	/* =========================================================
	 * TCP Socket states 
	 * http://lxr.linux.no/linux+v3.10.2/include/linux/tcp.h
	 * ========================================================= */
		
		//uint16_t	tcp_header_len;	/* Bytes of tcp header to send		*/

	/*
	 *	RFC793 variables by their proper names. This means you can
	 *	read the code and the spec side by side (and laugh ...)
	 *	See RFC793 and RFC1122. The RFC writes these in capitals.
	 */
	 	uint32_t	rcv_nxt;	/* What we want to receive next 	*/
		uint32_t	copied_seq;	/* Head of yet unread data		*/
		uint32_t	rcv_wup;	/* rcv_nxt on last window update sent	*/
	 	uint32_t	snd_nxt;	/* Next sequence we send		*/

	 	uint32_t	snd_una;	/* First byte we want an ack for	*/
	 	uint32_t	snd_sml;	/* Last byte of the most recently transmitted small packet */
		uint32_t	rcv_tstamp;	/* timestamp of last received ACK (for keepalives) */
		uint32_t	lsndtime;	/* timestamp of last sent data packet (for restart window) */

		uint32_t	tsoffset;	/* timestamp offset */


		uint32_t	snd_wl1;	/* Sequence for window update		*/
		uint32_t	snd_wnd;	/* The window we expect to receive	*/
		uint32_t	max_window;	/* Maximal window ever seen from peer	*/
		uint32_t	mss_cache;	/* Cached effective mss, not including SACKS */

		uint32_t	window_clamp;	/* Maximal window to advertise		*/
		uint32_t	rcv_ssthresh;	/* Current window clamp			*/

		uint16_t	advmss;		/* Advertised MSS			*/
		uint8_t	unused;
		uint8_t	nonagle     : 4,/* Disable Nagle algorithm?             */
			thin_lto    : 1,/* Use linear timeouts for thin streams */
			thin_dupack : 1,/* Fast retransmit on first dupack      */
			repair      : 1,
			frto        : 1;/* F-RTO (RFC5682) activated in CA_Loss */
		uint8_t	repair_queue;
		uint8_t	do_early_retrans:1,/* Enable RFC5827 early-retransmit  */
			syn_data:1,	/* SYN includes data */
			syn_fastopen:1,	/* SYN includes Fast Open option */
			syn_data_acked:1;/* data in SYN is acked by SYN-ACK */
		uint32_t	tlp_high_seq;	/* snd_nxt at the time of TLP retransmit. */

	/* RTT measurement */
		uint32_t	srtt;		/* smoothed round trip time << 3	*/
		uint32_t	mdev;		/* medium deviation			*/
		uint32_t	mdev_max;	/* maximal mdev for the last rtt period	*/
		uint32_t	rttvar;		/* smoothed mdev_max			*/
		uint32_t	rtt_seq;	/* sequence number to update rttvar	*/

		uint32_t	packets_out;	/* Packets which are "in flight"	*/
		uint32_t	retrans_out;	/* Retransmitted packets out		*/

		uint8_t	reordering;	/* Packet reordering metric.		*/

		uint8_t	keepalive_probes; /* num of allowed keep alive probes	*/

	/*
	 *	Slow start and congestion control (see also Nagle, and Karn & Partridge)
	 */
	 	uint32_t	snd_ssthresh;	/* Slow start size threshold		*/
	 	uint32_t	snd_cwnd;	/* Sending congestion window		*/
		uint32_t	snd_cwnd_cnt;	/* Linear increase counter		*/
		uint32_t	snd_cwnd_clamp; /* Do not allow snd_cwnd to grow above this */
		uint32_t	snd_cwnd_used;
		uint32_t	snd_cwnd_stamp;
		uint32_t	prior_cwnd;	/* Congestion window at start of Recovery. */
		uint32_t	prr_delivered;	/* Number of newly delivered packets to
					 * receiver in Recovery. */
		uint32_t	prr_out;	/* Total number of pkts sent during Recovery. */

	 	uint32_t	rcv_wnd;	/* Current receiver window		*/
		uint32_t	write_seq;	/* Tail(+1) of data held in tcp send buffer */
		uint32_t	pushed_seq;	/* Last pushed seq, required to talk to windows */
		uint32_t	lost_out;	/* Lost packets			*/
		uint32_t	sacked_out;	/* SACK'd packets			*/
		uint32_t	fackets_out;	/* FACK'd packets			*/
		uint32_t	tso_deferred;

		// TODO: SACK block readded?

		int     lost_cnt_hint;
		uint32_t     retransmit_high;	/* L-bits may be on up to this seqno */

		uint32_t	lost_retrans_low;	/* Sent seq after any rxmit (lowest) */

		uint32_t	prior_ssthresh; /* ssthresh saved at recovery start	*/
		uint32_t	high_seq;	/* snd_nxt at onset of congestion	*/

		uint32_t	retrans_stamp;	/* Timestamp of the last retransmit,
					 * also used in SYN-SENT to remember stamp of
					 * the first SYN. */
		uint32_t	undo_marker;	/* tracking retrans started here. */
		int	undo_retrans;	/* number of undoable retransmissions. */
		uint32_t	total_retrans;	/* Total retransmits for entire connection */

		unsigned int		keepalive_time;	  /* time before keep alive takes place */
		unsigned int		keepalive_intvl;  /* time interval between keep alive probes */

		int			linger2;

	/* Receiver side RTT estimation */
		struct {
			uint32_t	rtt;
			uint32_t	seq;
			uint32_t	time;
		} rcv_rtt_est;

	/* Receiver queue space */
		struct {
			int	space;
			uint32_t	seq;
			uint32_t	time;
		} rcvq_space;    
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
    
    //modify routing table
    void addRoute(const XID &sid) {
		String cmd=sid.unparse() + " " + String(DESTINED_FOR_LOCALHOST);
        HandlerCall::call_write(_routeTable, "add", cmd);
    }   
        
    void delRoute(const XID &sid) {
        String cmd= sid.unparse();
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
//    bool ProcessPollTimeout(unsigned short, PollEvent& pe);
    /*
    ** Xsockets API handlers
    */
    void Xsocket(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xsetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xgetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xbind(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xclose(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
    void Xconnect(unsigned short _sport, xia::XSocketMsg *xia_socket_msg);
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
};


CLICK_ENDDECLS

#endif
