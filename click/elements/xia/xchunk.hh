#ifndef CLICK_XCHUNK_HH
#define CLICK_XCHUNK_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/xiaheader.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>
#include <clicknet/xia.h>
#include "xiaxidroutetable.hh"
#include <click/string.hh>
#include <elements/ipsec/sha1_impl.hh>
#include <click/xiatransportheader.hh>
#include <clicknet/tcp.h>
#include "xtransport.hh"
#include "clicknet/tcp_fsm.h"


static u_char	tcp_outflags_c[TCP_NSTATES] = {
    TH_RST|TH_ACK, 0, TH_SYN, TH_SYN|TH_ACK,
    TH_ACK, TH_ACK,
    TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_FIN|TH_ACK, TH_ACK, TH_ACK,
};

extern int	tcp_backoff[TCP_MAXRXTSHIFT + 1];


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
#define XOPT_HLIM 0x07001
#define XOPT_NEXT_PROTO 0x07002

#ifndef DEBUG
#define DEBUG 0
#endif


#define UNUSED(x) ((void)(x))

#define ACK_DELAY           300
#define TEARDOWN_DELAY      240000
#define HLIM_DEFAULT        250
#define LAST_NODE_DEFAULT   -1
#define RANDOM_XID_FMT      "%s:30000ff0000000000000000000000000%08x"
#define UDP_HEADER_SIZE     8

#define NETWORK_PORT    2

#define MAX_TCPOPTLEN 32

#define TCP_REXMTVAL(tp) \
	(((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

/*
 * (BSD)
 * Flags used when sending segments in tcp_output.  Basic flags (TH_RST,
 * TH_ACK,TH_SYN,TH_FIN) are totally determined by state, with the proviso
 * that TH_FIN is sent only if all data queued for output is included in the
 * segment. See definition of flags in xiatransportheader.hh
 */
//static const uint8_t  tcp_outflags[TCP_NSTATES] = {
//      TH_RST|TH_ACK,      /* 0, CLOSED */
//      0,          /* 1, LISTEN */
//      TH_SYN,         /* 2, SYN_SENT */
//      TH_SYN|TH_ACK,      /* 3, SYN_RECEIVED */
//      TH_ACK,         /* 4, ESTABLISHED */
//      TH_ACK,         /* 5, CLOSE_WAIT */
//      TH_FIN|TH_ACK,      /* 6, FIN_WAIT_1 */
//      TH_FIN|TH_ACK,      /* 7, CLOSING */
//      TH_FIN|TH_ACK,      /* 8, LAST_ACK */
//      TH_ACK,         /* 9, FIN_WAIT_2 */
//      TH_ACK,         /* 10, TIME_WAIT */
//  };

#define TCPOUTFLAGS

CLICK_DECLS

class XChunk;
// Queue of packets from transport to socket layer
class CTCPQueue {

	class CTCPQueueElt {
	public:
		CTCPQueueElt(WritablePacket *p, tcp_seq_t s, tcp_seq_t n) {
			_p = p;
			seq = s;
			seq_nxt = n;
			nxt = NULL;
		}

		~CTCPQueueElt() {};
		WritablePacket 	*_p;
		CTCPQueueElt 	*nxt;
		tcp_seq_t		seq;
		tcp_seq_t		seq_nxt;
	};

public:
	CTCPQueue(XChunk *con);
	CTCPQueue(){};
	~CTCPQueue();

	int push(WritablePacket *p, tcp_seq_t seq, tcp_seq_t seq_nxt);
	void loop_last();
	WritablePacket *pull_front();

	// @Harald: Aren't all of these seq num arithmetic operations unsafe from
	// wraparound ?
	tcp_seq_t first() { return _q_first ? _q_first->seq : 0; }
	tcp_seq_t first_len() { return _q_first ? (_q_first->seq_nxt - _q_first->seq) : 0; }
	tcp_seq_t expected() { return _q_tail ? _q_tail->seq_nxt : 0; }
	tcp_seq_t tailseq() { return _q_tail ? _q_tail->seq : 0; }
	tcp_seq_t last()  { return _q_last ? _q_last->seq : 0; }
	tcp_seq_t last_nxt()  { return _q_last ? _q_last->seq_nxt : 0; }
	tcp_seq_t bytes_ok() { return _q_last ? _q_last->seq - _q_first->seq : 0; }
	bool is_empty() { return _q_first ? false : true; }
	//FIXME: Returns true even if there is a hole at the front! Decide whether
	//to rethink what we mean by "ordered"
	bool is_ordered() { return (_q_last == _q_tail); }

	StringAccum * pretty_print(StringAccum &sa, int width);

private:
	int verbosity();
	XChunk *_con;   /* The XChunk to which I belong */

	CTCPQueueElt *_q_first; /* The first segment in the queue
							 (a.k.a. the head element) */
	CTCPQueueElt *_q_last;  /* The last segment of ordered data
							 in the queue (a.k.a. the last
							 segment before a gap occurs)  */
	CTCPQueueElt *_q_tail;   /* The very last segment in the queue
							 (a.k.a. the next expected in-order
							 ariving segment should be inserted
							 after this segment )  */
};

// Queue of packets from socket layer to transport
class CTCPFifo
{
public:
#define FIFO_SIZE 256
	CTCPFifo(XChunk *con);
	CTCPFifo(){};
	~CTCPFifo();
	int 	push(WritablePacket *);
	int 	pkt_length() { return (_head - _tail) % FIFO_SIZE; }
	bool 	is_empty() { return ( 0 == pkt_length()) ; }
	int 	pkts_to_send(int offset, int win);
	void 	drop_until (tcp_seq_t offset);

	tcp_seq_t byte_length() { return _bytes; }
	WritablePacket *pull();
	WritablePacket *get (tcp_seq_t offset);

protected:
	WritablePacket **_q;
	int 	_head;
	int 	_tail;
	int 	_peek_cache_position;
	int 	_peek_cache_offset;
	tcp_seq_t _bytes;

private:
	XChunk *_con;   /* The XChunk to which I belong */
	int verbosity();
};

class XChunk  : public sock {

public:
	XChunk(XTRANSPORT *transport, unsigned short port);
	XChunk(){};
	~XChunk() {};
	int read_from_recv_buf(XSocketMsg *xia_socket_msg);
	void check_for_and_handle_pending_recv();
	/* TCP related core functions */
	void 	tcp_input(WritablePacket *p);
	void 	tcp_output();
	int		usrsend(WritablePacket *p);
	void    usrclosed() ;
	void 	usropen();
	void	tcp_timers(int timer);
	void 	fasttimo();
	void 	slowtimo();
	void push(Packet *_p);
	int verbosity();
#define SO_STATE_HASDATA	0x01
#define SO_STATE_ISCHOKED   0x10

	// short state() const { return tp->t_state; }
	bool has_pullable_data() { return !_q_recv.is_empty() && SEQ_LT(_q_recv.first(), tp->rcv_nxt); }
	void print_state(StringAccum &sa);

    // XTRANSPORT *get_transport() { return transport; }
	tcpcb 		*tp;
	sock *listening_sock;
private:
    void set_state(const HandlerState s);

	void 		_tcp_dooptions(u_char *cp, int cnt, uint8_t th_flags,
	int * ts_present, u_long *ts_val, u_long *ts_ecr);
	void 		tcp_respond(tcp_seq_t ack, tcp_seq_t seq, int flags);
	void		tcp_setpersist();
	void		tcp_drop(int err);
	void		tcp_xmit_timer(short rtt);
	void 		tcp_canceltimers();
	u_int		tcp_mss(u_int);
	tcpcb*		tcp_newtcpcb();
	tcp_seq_t	so_recv_buffer_space();
	inline void tcp_set_state(short);
	inline void print_tcpstats(WritablePacket *p, const char *label);
	short tcp_state() const { return tp->t_state; }


	CTCPFifo		_q_usr_input;
	CTCPQueue	_q_recv;
	tcp_seq_t	so_recv_buffer_size;
	int			_so_state;

} ;

/* THE method where we register, and handle any TCP State Updates */
inline void
XChunk::tcp_set_state(short state) {

	tp->t_state = state;
	// debug_output(VERB_STATES, "[%s] Flow: [%s]: State: [%s]->[%s]", get_transport()->name().c_str(), sa.c_str(), tcpstates[old], tcpstates[tp->t_state]);

	/* Set stateless flags which will dispatch the appropriately flagged
	 * signal packets into the mesh when we enter into one of these
	 * following states
	 */

	/* stateless flags are disabled for now untill a better
	 * way of handling those is found */
	switch (state) {
	case TCPS_CLOSED:
		set_state(CLOSE);
		get_transport() -> ChangeState(this, CLOSED);
		printf("\t\t\t\tchanged to be reaped\n");
		reap = true;
		break;
	case TCPS_LISTEN:
		get_transport() -> ChangeState(this, LISTEN);
		break;
	case TCPS_SYN_SENT:
		get_transport() -> ChangeState(this, SYN_SENT);
		break;
	case TCPS_SYN_RECEIVED:
		get_transport() -> ChangeState(this, SYN_RCVD);
		break;
	case TCPS_ESTABLISHED:
		get_transport() -> ChangeState(this, CONNECTED);
		set_state(ACTIVE);
		break;
	case TCPS_CLOSE_WAIT:
		get_transport() -> ChangeState(this, CLOSE_WAIT);
		set_state(SHUTDOWN);
		break;
	case TCPS_FIN_WAIT_1:
		get_transport() -> ChangeState(this, FIN_WAIT1);
		set_state(SHUTDOWN);
		break;
	case TCPS_CLOSING:
		get_transport() -> ChangeState(this, CLOSING);
		break;
	case TCPS_LAST_ACK:
		get_transport() -> ChangeState(this, LAST_ACK);
		set_state(SHUTDOWN);
		break;
	case TCPS_FIN_WAIT_2:
		get_transport() -> ChangeState(this, FIN_WAIT2);
		break;
	case TCPS_TIME_WAIT:
		get_transport() -> ChangeState(this, TIME_WAIT);
		break;
	}
};


inline int
XChunk::verbosity()  { return get_transport()->verbosity(); }

inline int
CTCPQueue::verbosity() { return _con->sock::get_transport()->verbosity(); }

inline int
CTCPFifo::verbosity()  { return _con->get_transport()->verbosity(); }



CLICK_ENDDECLS

#endif
