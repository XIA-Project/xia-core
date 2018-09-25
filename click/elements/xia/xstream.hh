#ifndef CLICK_XSTREAM_HH
#define CLICK_XSTREAM_HH

#define CLICK_XIA

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
#include <click/xiastreamheader.hh>
#include "xtransport.hh"
#include <clicknet/tcp_fsm.h>
#include <cstdint> // uint32_t
#include <click/task.hh>

#include "taskident.hh"

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
#define XOPT_HLIM		0x07001
#define XOPT_NEXT_PROTO	0x07002

#ifndef DEBUG
#define DEBUG 0
#endif


#define UNUSED(x) ((void)(x))

#define ACK_DELAY			300
#define TEARDOWN_DELAY		240000
#define RANDOM_XID_FMT		"%s:30000ff0000000000000000000000000%08x"
#define UDP_HEADER_SIZE		8

// was 32, changed to match XTCP_OPTIONS_MAX from clicknet/xtcp.h
#define MAX_TCPOPTLEN		XTCP_OPTIONS_MAX

#define TCP_REXMTVAL(tp) \
	(((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

/*
 * (BSD)
 * Flags used when sending segments in tcp_output.  Basic flags (TH_RST,
 * TH_ACK,TH_SYN,TH_FIN) are totally determined by state, with the proviso
 * that TH_FIN is sent only if all data queued for output is included in the
 * segment. See definition of flags in xtcp.hh
 */
//static const uint8_t  tcp_outflags[TCP_NSTATES] = {
//	  TH_RST|TH_ACK,	  /* 0, CLOSED */
//	  0,		  /* 1, LISTEN */
//	  TH_SYN,		 /* 2, SYN_SENT */
//	  TH_SYN|TH_ACK,	  /* 3, SYN_RECEIVED */
//	  TH_ACK,		 /* 4, ESTABLISHED */
//	  TH_ACK,		 /* 5, CLOSE_WAIT */
//	  TH_FIN|TH_ACK,	  /* 6, FIN_WAIT_1 */
//	  TH_FIN|TH_ACK,	  /* 7, CLOSING */
//	  TH_FIN|TH_ACK,	  /* 8, LAST_ACK */
//	  TH_ACK,		 /* 9, FIN_WAIT_2 */
//	  TH_ACK,		 /* 10, TIME_WAIT */
//  };

#define TCPOUTFLAGS

CLICK_DECLS

class XStream;
// Queue of packets from transport to socket layer
class TCPQueue {

	class TCPQueueElt {
	public:
		TCPQueueElt(char *p, uint32_t plen, tcp_seq_t s, tcp_seq_t n) {
			_p = p;
			_plen = plen;
			seq = s;
			seq_nxt = n;
			nxt = NULL;
		}

		~TCPQueueElt() {};
		char 	*_p;
		uint32_t _plen;
		TCPQueueElt 	*nxt;
		tcp_seq_t		seq;
		tcp_seq_t		seq_nxt;
	};

public:
	TCPQueue(XStream *con);
	TCPQueue(){};
	~TCPQueue();

	int push(char *p, uint32_t plen, tcp_seq_t seq, tcp_seq_t seq_nxt);
	void loop_last();
	char *pull_front(uint32_t *len);

	// @Harald: Aren't all of these seq num arithmetic operations unsafe from
	// wraparound ?
	tcp_seq_t first() { return _q_first ? _q_first->seq : 0; }
	tcp_seq_t first_len() { return _q_first ? (_q_first->seq_nxt - _q_first->seq) : 0; }
	tcp_seq_t expected() { return _q_tail ? _q_tail->seq_nxt : 0; }
	tcp_seq_t tailseq() { return _q_tail ? _q_tail->seq : 0; }
	tcp_seq_t last()  { return _q_last ? _q_last->seq : 0; }
	tcp_seq_t last_nxt()  { return _q_last ? _q_last->seq_nxt : 0; }
	tcp_seq_t bytes_ok() { return (_q_last && _q_first) ? _q_last->seq - _q_first->seq : 0; }
	bool is_empty() { return _q_first ? false : true; }
	//FIXME: Returns true even if there is a hole at the front! Decide whether
	//to rethink what we mean by "ordered"
	bool is_ordered() { return (_q_last == _q_tail); }

	StringAccum * pretty_print(StringAccum &sa, int width);

private:
	int verbosity();
	XStream *_con;   /* The XStream to which I belong */

	TCPQueueElt *_q_first; /* The first segment in the queue
							 (a.k.a. the head element) */
	TCPQueueElt *_q_last;  /* The last segment of ordered data
							 in the queue (a.k.a. the last
							 segment before a gap occurs)  */
	TCPQueueElt *_q_tail;   /* The very last segment in the queue
							 (a.k.a. the next expected in-order
							 ariving segment should be inserted
							 after this segment )  */
};

// FIXME: this should really be a socket option
#define TCP_FIFO_SIZE (256 * 1024)

// Queue of packets from socket layer to transport
class TCPFifo
{
public:
	TCPFifo(unsigned size);
	~TCPFifo();

	unsigned available();	// return amount of free space
	void clear();			// empty the buffer

	bool is_empty()         { return (_used == 0); };
	tcp_seq_t byte_length() { return _used; }

	int push(WritablePacket *);

	// FIXME: this is packet rather than byte based, should fix in the future
	int pkts_to_send(int offset, int win);

	WritablePacket *get(tcp_seq_t offset, unsigned len);
	void drop_until (tcp_seq_t offset);

private:
	unsigned _size;
	unsigned _used;
	unsigned _free;

	unsigned char *_buf;
	unsigned char *_h;
	unsigned char *_t;
	unsigned char *_last;
};

class XStream  : public sock {

public:
	XStream(XTRANSPORT *transport, unsigned short port, uint32_t id);
	XStream() : _q_usr_input(TCP_FIFO_SIZE), _outputTask(NULL, 0) {};
	~XStream() {};

	const char *class_name() const  { return "XStream"; }

	bool run_task(Task*);

	int read_from_recv_buf(XSocketMsg *xia_socket_msg);
	void check_for_and_handle_pending_recv();
	/* TCP related core functions */
	void 	tcp_input(WritablePacket *p);
	void	tcp_output();
	int		usrsend(WritablePacket *p);
	void	usrmigrate();
	void	usrclosed() ;
	void 	usropen();
	void	tcp_timers(int timer);
	void 	fasttimo();
	void 	slowtimo();
	void push(Packet *_p);
	int verbosity();
#define SO_STATE_HASDATA	0x01
#define SO_STATE_ISCHOKED   0x10

	// holding area for one data packet if the send buffer is full
	bool stage_data(WritablePacket *p, unsigned seq);
	WritablePacket *unstage_data();

	// short state() const { return tp->t_state; }
	bool has_pullable_data() { return !_q_recv.is_empty() && SEQ_LT(_q_recv.first(), tp->rcv_nxt); }
	void print_state(StringAccum &sa);

	// XTRANSPORT *get_transport() { return transport; }
	tcpcb 		*tp;
	sock *listening_sock;

private:
	void set_state(const HandlerState s);

	void 		_tcp_dooptions(const u_char *cp, int cnt, uint8_t th_flags,
	int * ts_present, uint32_t *ts_val, uint32_t *ts_ecr);
	void 		tcp_respond(tcp_seq_t seq, tcp_seq_t ack, int flags);
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
	tcp_seq _tcp_iss();


	TCPQueue	_q_recv;
	TCPFifo		_q_usr_input;
	tcp_seq_t	so_recv_buffer_size;
	int			_so_state;

	// holding area for extra data when the api doesn't ask for a full packet
	char *_tail;
	unsigned _tail_length;

	// holding location for when xmit buffer is full and we are blocking
	WritablePacket *_staged;
	unsigned _staged_seq;

	TaskIdent _outputTask;
};

/* THE method where we register, and handle any TCP State Updates */
inline void
XStream::tcp_set_state(short state) {

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
		//printf("\t\t\t\tchanged to be reaped\n");
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
XStream::verbosity()  { return get_transport()->verbosity(); }

inline int
TCPQueue::verbosity() { return _con->sock::get_transport()->verbosity(); }

inline unsigned TCPFifo::available()
{
	printf("%u bytes left free\n", _free);
	return _free;
}


inline void TCPFifo::clear()
{
	_h = _t = _buf;
	_used = 0;
	_free = _size;
}


CLICK_ENDDECLS

#endif
