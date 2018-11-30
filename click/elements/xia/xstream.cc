#include "../../userlevel/xia.pb.h"
#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>
#include <click/xiasecurity.hh>
#include <click/xiamigrate.hh>
#include <click/standard/scheduleinfo.hh> /* ScheduleInfo */

#include <fcntl.h>
#include <cstdint> // uint32_t
#include <cassert>

#include "xstream.hh"
#include "xtransport.hh"
#include "xlog.hh"

// FIXME: we shouldn't have 2 different copies of the so_error variable.
// right now it's in the tcp control block, and the sock class.


#define TCPTIMERS
#define TCPOUTFLAGS
#define TCPSTATES

#define UNUSED(x) ((void)(x))
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define TCP_PAWS_IDLE	(24 * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)

#define _tcp_rcvseqinit(tp) \
	(tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define _tcp_sendseqinit(tp) \
	(tp)->snd_una = (tp)->snd_nxt = (tp)->snd_max = \
(tp)->snd_up = (tp)->iss

static u_char	tcp_outflags[TCP_NSTATES] = {
	XTH_RST|XTH_ACK, 0, XTH_SYN, XTH_SYN|XTH_ACK,
	XTH_ACK, XTH_ACK,
	XTH_FIN|XTH_ACK, XTH_FIN|XTH_ACK, XTH_FIN|XTH_ACK, XTH_ACK, XTH_ACK,
};

//static unsigned long startMillis = 1476985789000;


CLICK_DECLS

int isPowerOfTwo (unsigned int x){

	while (((x % 2) == 0) && x > 1) /* While x is even and > 1 */
		x /= 2;
	return (x == 1);
}

void
XStream::push(Packet *_p) {
	WritablePacket *p = _p->uniqueify();
	tcp_input(p);
}

tcp_seq XStream::_tcp_iss()
{
	// create a random starting sequence #
	tcp_seq iss;
	int r  = open("/dev/urandom", O_RDONLY);
	int rc = read(r, &iss, sizeof(iss));
	close(r);

	// this ought not to happen
	if (rc != sizeof(iss)) {
		iss = 0x011111;
	}

	//printf("iss = %u\n", iss);
	return iss;
}



inline void
XStream::print_tcpstats(WritablePacket *p, const char* label)
{
	UNUSED(p);
	UNUSED(label);
 //	const xtcp *tcph= p->tcp_header();
 //	const click_ip 	*iph = p->ip_header();
	// int len = ntohs(iph->ip_len) - sizeof(click_ip) - (tcph->th_off << 2);

	//debug_output(VERB_TCPSTATS, "[%s] [%s] S/A: [%u/%u] len: [%u] 59: [%u] 60: [%u] 62: [%u] 63: [%u] 64: [%u] 65: [%u] 67: [%u] 68: [%u] 80:[%u] 81:[%u] fifo: [%u] q1st/len: [%u/%u] qlast: [%u] qbtok: [%u] qisord: [%u]", SPKRNAME, label, ntohl(tcph->th_seq), ntohl(tcph->th_ack), len, tp->snd_una, tp->snd_nxt, tp->snd_wl1, tp->snd_wl2, tp->iss, tp->snd_wnd, tp->rcv_wnd, tp->rcv_nxt, tp->snd_cwnd, tp->snd_ssthresh, _q_usr_input.byte_length(), _q_recv.first(), _q_recv.first_len(), _q_recv.last(), _q_recv.bytes_ok(), _q_recv.is_ordered());
}


// Stateful TCP segment input (recvd packet) handling
void
XStream::tcp_input(WritablePacket *p)
{

	unsigned 	tiwin, tiflags;
	uint32_t		ts_val, ts_ecr;
	//int			len, tlen; /* seems to be unused */
	unsigned	off, optlen;
	const u_char *optp;
	int		ts_present = 0;
	int 	iss = 0;
	int 	todrop, acked, needoutput = 0;
	bool ourfinisacked;
	struct 	mini_tcpip  ti;
	XID 	source_xid;
	XID 	destination_xid;
	XIDpair 	xid_pair;
//  tcp_seq_t	tseq;
	XIAHeader xiah(p->xia_header());
	StreamHeader thdr(p);

	xtcp *tcph= (xtcp *)thdr.header();
	if (tcph == NULL)
	{
		click_chatter("Invalid header\n");
		return;
	}

	get_transport()->_tcpstat.tcps_rcvtotal++;

	/* we need to copy ti, since we need it later */
	ti.ti_seq = ntohl(tcph->th_seq);
	ti.ti_ack = ntohl(tcph->th_ack);
	ti.ti_off = tcph->th_off;
	ti.ti_flags = ntohs(tcph->th_flags);
	ti.ti_win = ntohl(tcph->th_win);
	ti.ti_len = (uint16_t)(thdr.plen());

	/*205 packet should be sane, skip tests */
	off = ti.ti_off << 2;

	// DRB: why is this test disabled???
	if (0&&off < sizeof(xtcp)) {
		get_transport()->_tcpstat.tcps_rcvbadoff++;
		p->kill();
		return;
	}
	// ti.ti_len -= sizeof(xtcp) + off;

	if (tp->so_flags & SO_FIN_AFTER_TCP_IDLE){
		tp->t_timer[TCPT_IDLE] = get_transport()->globals()->so_idletime;
	}

	/*237*/
	optlen = off - sizeof(xtcp);
	optp = thdr.tcpopt();

	/* TODO Timestamp prediction */

	/*257*/
	tiflags = ti.ti_flags;

	/*293*/
	if ((tiflags & XTH_SYN) == 0){
		tiwin = ti.ti_win << tp->snd_scale;
	} else{
		tiwin = ti.ti_win;
	}

	/*334*/
	tp->t_idle = 0;
	tp->t_timer[TCPT_KEEP] = get_transport()->globals()->tcp_keepidle;

	/*344*/
	_tcp_dooptions(optp, optlen, ti.ti_flags, &ts_present, &ts_val, &ts_ecr);

	/*347 TCP "Fast Path" packet processing */

	/*
	 * Header prediction: check for the two common cases of a uni-directional
	 * data xfer.  If the packet has no control flags, is in-sequence, the
	 * window didn't change and we're not retransmitting, it's a candidate.  If
	 * the length is zero and the ack moved forward, we're the sender side of
	 * the xfer.  Just free the data acked & wake any higher level process that
	 * was blocked waiting for space.  If the length is non-zero and the ack
	 * didn't move, we're the receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to the socket buffer and
	 * note that we need a delayed ack.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
		(tiflags & (XTH_SYN|XTH_FIN|XTH_RST|XTH_ACK)) == XTH_ACK &&
		(!ts_present || TSTMP_GEQ(ts_val, tp->ts_recent)) &&
		ti.ti_seq == tp->rcv_nxt &&
		tiwin &&
		tiwin == tp->snd_wnd &&
		tp->snd_nxt == tp->snd_max){ // tcp fast path


			/* If last ACK falls within this segment's sequence numbers,
			 *  record the timestamp. */
			if (ts_present && SEQ_LEQ(ti.ti_seq, tp->last_ack_sent) &&
				SEQ_LT(tp->last_ack_sent, ti.ti_seq + ti.ti_len)){
				tp->ts_recent_age = get_transport()->tcp_now();
				tp->ts_recent = ts_val;
			}

			if (ti.ti_len == 0){ // pure ack for outstanding data

				if (SEQ_GT(ti.ti_ack, tp->snd_una) &&
					SEQ_LEQ(ti.ti_ack, tp->snd_max) &&
					tp->snd_cwnd >= tp->snd_wnd){

					++(get_transport()->_tcpstat.tcps_predack);
					if (ts_present){
						tcp_xmit_timer((get_transport()->tcp_now()) - ts_ecr+1);

					} else if (tp->t_rtt && SEQ_GT(ti.ti_ack, tp->t_rtseq)){
						tcp_xmit_timer(tp->t_rtt);
					}

					acked = ti.ti_ack - tp->snd_una;
					(get_transport()->_tcpstat.tcps_rcvackpack)++;
					get_transport()->_tcpstat.tcps_rcvackbyte += acked;

					// We can now drop data we know was recieved by the other side
					_q_usr_input.drop_until(acked);
					tp->snd_una = ti.ti_ack;

					if (_staged){ // because drop_until frees up queue space
						unstage_data();
					}

					/* If all outstanding data are acked, stop
					 * retransmit timer, otherwise restart timer
					 * using current (possibly backed-off) value.
					 * If process is waiting for space,
					 * wakeup/selwakeup/signal.  If data
					 * are ready to send, let tcp_output
					 * decide between more output or persist.
					 */
					if (tp->snd_una == tp->snd_max){
						tp->t_timer[TCPT_REXMT] = 0;
					} else if (tp->t_timer[TCPT_PERSIST] == 0){
						tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
					}

					if (! _q_usr_input.is_empty()){
						tcp_output();
					}

					return;
				}
			} else if (ti.ti_ack == tp->snd_una &&
				(_q_recv.is_empty() || _q_recv.is_ordered()) &&
				(so_recv_buffer_size > _q_recv.bytes_ok() + ti.ti_len)) {
				/* this is a pure, in-sequence data packet
				 * where the reassembly queue is empty or in order and
				 * we have enough buffer space to take it.
				 */

				//debug_output(VERB_TCP, "[%s] got pure data: [%u]", SPKRNAME, ti.ti_seq);
				//debug_output(VERB_TCPSTATS, "input (fp) updating rcv_nxt [%u] -> [%u]", tp->rcv_nxt, tp->rcv_nxt + ti.ti_len);
				assert(ti.ti_seq == tp->rcv_nxt);
				tp->rcv_nxt += ti.ti_len;

				// Update some TCP Stats
				++(get_transport()->_tcpstat.tcps_preddat);
				(get_transport()->_tcpstat.tcps_rcvpack)++;
				get_transport()->_tcpstat.tcps_rcvbyte += ti.ti_len;

				/* Drop TCP/IP hdrs and TCP opts, add data to recv queue. */
				char *pkt = (char *) malloc(ti.ti_len);
				assert(pkt);
				memcpy(pkt, (char *)thdr.payload(), ti.ti_len);

				/* _q_recv.push() corresponds to the tcp_reass function whose purpose is
				 * to put all data into the TCPQueue for both possible reassembly and
				 * in-order presentation to the "application socket" which in the
				 * TCPget_transport is the stateless pull port. */
				if (_q_recv.push(pkt, ti.ti_len, ti.ti_seq, ti.ti_seq + ti.ti_len) < 0){
					//debug_output(VERB_ERRORS, "Fast Path segment push into q_recv FAILED");
					if(pkt != NULL) {
						free(pkt);
						pkt = NULL;
					}
				} else if (!_q_recv.is_empty() && has_pullable_data()){
					// If the reassembly queue has data, a gap should have just been
					// filled - then we set rcv_next to the last seq num in the queue to
					// indicate the next packet we expect to get from the sender.

					tp->rcv_nxt = _q_recv.last_nxt();
					check_for_and_handle_pending_recv();
					//debug_output(VERB_TCPSTATS, "input (fp) updating rcv_nxt to [%u]", tp->rcv_nxt);
				}

				tp->t_flags |= TF_DELACK;
				tcp_output();

				if (p != NULL){
					p->kill();
				}

				return;
			}
		}

	// DRB
	//print_tcpstats(p, "tcp_input (sp)");
	/* 438 TCP "Slow Path" processing begins here */
	char *pkt = NULL;
	uint32_t pktlen = ti.ti_len;
	if (pktlen){
		pkt = (char *)malloc(pktlen);
		assert(pkt);
		memcpy(pkt, (char*)thdr.payload(), ti.ti_len);
	}

	int win = so_recv_buffer_space();
	if (win < 0) { win = 0; }
	tp->rcv_wnd = max(win, (int)(tp->rcv_adv - tp->rcv_nxt));

	/* 456 Transitioning FROM tp->t_state TO... */
	switch (tp->t_state){
		case TCPS_CLOSED:
		case TCPS_LISTEN: // this is where we process SYN packets
			/* If the RST flag is set */
			if (tiflags & XTH_RST)
				goto drop;
			/* If ACK is set */
			if (tiflags & XTH_ACK)
				goto dropwithreset;
			/* If the SYN flag is not set exclusively */
			if (!(tiflags & XTH_SYN))
				goto drop;

			/*479 no need to do socket stuff */

			/* 506 we don't use a template */

			/* 512 we have handled to options already */

			/* 515 */
			if (iss) {
				tp->iss = iss;
			} else {
				tp->iss = _tcp_iss(); // get a random starting sequence #
			}
			tp->irs = ti.ti_seq;
			_tcp_sendseqinit(tp);
			_tcp_rcvseqinit(tp);
			tp->t_flags |= TF_ACKNOW; // must send synack!
			tcp_set_state(TCPS_SYN_RECEIVED);
			tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
			get_transport()->_tcpstat.tcps_accepts++;

			// If SYN received via RV service, send migrate option to client
			// so client has our new address.
			if (rv_modified_dag) {
				migrating = true;
			}

			// // If the app is ready for a new connection, alert it
			// XSocketMsg *acceptXSM = sk->pendingAccepts.front();
			// get_transport()->ReturnResult(_dport, acceptXSM);
			// sk->pendingAccepts.pop();
			// delete acceptXSM;

			goto trimthenstep6;

			/* 530 */
		case TCPS_SYN_SENT: // this is where we process synack
			if ((tiflags & TH_ACK) &&
				(SEQ_LEQ(ti.ti_ack, tp->iss) ||
				 SEQ_GT(ti.ti_ack, tp->snd_max))){
				goto dropwithreset;
			}

			if (tiflags & TH_RST){
				if (tiflags & TH_ACK){
					printf("tcp_drop #1\n");
					tcp_drop(ECONNREFUSED);
				}
				goto drop;
			}

			if ((tiflags & TH_SYN) == 0){ // must have syn flag to be a synack!
				goto drop;
			}

			/* 554 */
			if (tiflags & TH_ACK){
				tp->snd_una = ti.ti_ack;
				if (SEQ_LT(tp->snd_nxt, tp->snd_una)){
					tp->snd_nxt = tp->snd_una;
				}
			}
			tp->t_timer[TCPT_REXMT] = 0;
			tp->irs = ti.ti_seq;
			_tcp_rcvseqinit(tp);
			tp->t_flags |= TF_ACKNOW;
			if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)){
				get_transport() -> ChangeState(this, CONNECTED);
				tcp_set_state(TCPS_ESTABLISHED); // 3-way handshake complete

				// Notify API that the connection is established
				tp->so_error = so_error = 0;
				if (isBlocking) {
					XSocketMsg xsm;
					xsm.set_type(XCONNECT);
					xsm.set_id(get_id());
					xsm.set_sequence(0); // TODO: what should this be?
					xia::X_Connect_Msg *connect_msg = xsm.mutable_x_connect();
					connect_msg->set_ddag(src_path.unparse().c_str());
					connect_msg->set_status(X_Connect_Msg::XCONNECTED);

					get_transport()->ReturnResult(port, &xsm);
				}

				if (polling){
					// tell API we are writble now
					get_transport()->ProcessPollEvent(get_id(), POLLOUT);
				}

				/* Apply Window Scaling Options if set in incoming header */
				if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
					(TF_RCVD_SCALE | TF_REQ_SCALE)){
					tp->snd_scale = tp->requested_s_scale;
					tp->rcv_scale = tp->request_r_scale;
				}

				/* Record the RTT if set in incoming header */
				if (tp->t_rtt){
					tcp_xmit_timer(tp->t_rtt);
				}
			} else{
				tcp_set_state(TCPS_SYN_RECEIVED);
			}

			/* 583 */
	trimthenstep6:
		ti.ti_seq++;
		/* we don't accept half of a packet */
		if (ti.ti_len > tp->rcv_wnd){
			goto dropafterack;
		}
		tp->snd_wl1 = ti.ti_seq - 1;	// @Harald: I noticed that these values were +1 greater than in the book
		tp->rcv_up = ti.ti_seq + 1;     // rui: the only time rcv_up shows up. It's not used for anything!
		goto step6;

	} // switch case end



	/* 602 */
	/* timestamp processing */
	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check timestamp, if present.
	 * Then check that at least some bytes of segment are within
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 *
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if (ts_present && (tiflags & TH_RST) == 0 && tp->ts_recent &&
		TSTMP_LT(ts_val, tp->ts_recent)){

		// print_tcpstats(p, "tcp_input:ts - dan doesn't expect code execution to reach here unless connection is VERY old");
		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(get_transport()->tcp_now() - tp->ts_recent_age) > TCP_PAWS_IDLE){
			/*
			 * Invalidate ts_recent.  If this segment updates ts_recent, the age
			 * will be reset later and ts_recent will get a valid value.  If it
			 * does not, setting ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp echo reply when
			 * ts_recent isn't valid.  The age isn't reset until we get a valid
			 * ts_recent because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else{
			get_transport()->_tcpstat.tcps_rcvduppack++;
			get_transport()->_tcpstat.tcps_rcvdupbyte += ti.ti_len;
			get_transport()->_tcpstat.tcps_pawsdrop++;
			goto dropafterack;
		}
	}

	/* 635 646 */
	/* Determine if we need to trim the head off of an incoming segment */
	todrop = tp->rcv_nxt - ti.ti_seq;

	// todrop is > 0 IF the incoming segment begins prior to the end of the last
	// recieved segment (a.k.a. tp->rcv_nxt)
	if (todrop > 0){
		if (tiflags & TH_SYN){
				tiflags &= ~TH_SYN;
			ti.ti_seq++;
			todrop --;
		}
		if (todrop >= ti.ti_len){
			get_transport()->_tcpstat.tcps_rcvduppack++;
			get_transport()->_tcpstat.tcps_rcvdupbyte += ti.ti_len;

			if ((tiflags & XTH_FIN && todrop == ti.ti_len + 1)){
				todrop = ti.ti_len;
				tiflags &= ~XTH_FIN;
				tp->t_flags |= TF_ACKNOW;
			} else{
				if (todrop != 0 || (tiflags & XTH_ACK) == 0){
					goto dropafterack;
				}
			}
		} else{
			get_transport()->_tcpstat.tcps_rcvpartduppack++;
			get_transport()->_tcpstat.tcps_rcvpartdupbyte += todrop;
		}

		//printf("becareful 465\n");
		if (pkt != NULL){
			memmove(pkt, pkt+todrop, pktlen-todrop);
			pktlen = pktlen-todrop;
		}
		ti.ti_seq += todrop;
		ti.ti_len -= todrop;
	}

	/* 687 */
	/* drop after socket close */
	if (tp->t_state > TCPS_CLOSE_WAIT && ti.ti_len){
		tcp_set_state(TCPS_CLOSED);
		get_transport()->_tcpstat.tcps_rcvafterclose++;
		goto dropwithreset;
	}

	/* 697 More segment trimming: If segment ends after window, drop trailing
	 * data (and PUSH and FIN); if nothing left, just ACK. */
	todrop = (ti.ti_seq+ti.ti_len) - (tp->rcv_nxt+tp->rcv_wnd);

	if (todrop > 0){
		get_transport()->_tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= ti.ti_len){
			get_transport()->_tcpstat.tcps_rcvbyteafterwin += ti.ti_len;
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			/*FIXME: */
			/*
			if (tiflags & TH_SYN &&
				tp->t_state == TCPS_TIME_WAIT &&
				SEQ_GT(ti->ti_seq, tp->rcv_nxt)) {
				iss = tp->rcv_nxt + TCP_ISSINCR;

				tp = tcp_close(tp);
				goto findpcb;
			}
			*/
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && ti.ti_seq == tp->rcv_nxt){
				tp->t_flags |= TF_ACKNOW;
				get_transport()->_tcpstat.tcps_rcvwinprobe++;
			} else{
				goto dropafterack;
			}
		} else{
			get_transport()->_tcpstat.tcps_rcvbyteafterwin += todrop;
		}
		//printf("becareful 535\n");
		memmove(pkt, pkt+todrop, pktlen-todrop);
		pktlen = pktlen-todrop;
		ti.ti_len -= todrop;
		tiflags &= ~(XTH_PUSH|XTH_FIN);
	}

	/*737*/
	/* record timestamp*/
	if (ts_present && SEQ_LEQ(ti.ti_seq, tp->last_ack_sent) &&
		SEQ_LT(tp->last_ack_sent, ti.ti_seq + ti.ti_len + 1/*
		((tiflags & (XTH_SYN|XTH_FIN)) != 0) rui */)){
		tp->ts_recent_age = get_transport()->tcp_now();
		tp->ts_recent = ts_val;
	}

	/* 747 process RST */
	/*
	 * If the RST bit is set examine the state:
	 *	SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *	ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *	CLOSING, LAST_ACK, TIME_WAIT STATES
	 *	Close the tcb.
	 */
	if (tiflags & XTH_RST){
		//printf("551\n");
		switch (tp->t_state) {
		case TCPS_SYN_RECEIVED:
			tp->so_error = so_error = ECONNREFUSED;
			goto close;
		case TCPS_ESTABLISHED:
		case TCPS_FIN_WAIT_1:
		case TCPS_FIN_WAIT_2:
		case TCPS_CLOSE_WAIT:
			tp->so_error = so_error = ECONNRESET;
		close:
			tp->t_state = TCPS_CLOSED;
			get_transport()->_tcpstat.tcps_drops++;
			tcp_set_state(TCPS_CLOSED);
			goto drop;
		case TCPS_CLOSING:
		case TCPS_LAST_ACK:
		case TCPS_TIME_WAIT:
			tcp_set_state(TCPS_CLOSED);
			goto drop;
		}
	}

	/* 778 */
	/* drop SYN or !ACK during connection */
	if (tiflags & XTH_SYN) {
	printf("tcp_drop #2\n");
		tcp_drop(ECONNRESET);
		goto dropwithreset;
	}

	assert(tiflags & XTH_ACK);

	/* 791 ack processing */
	switch (tp->t_state) { //3-way handshake ack processing
	case TCPS_SYN_RECEIVED:
		if (SEQ_GT(tp->snd_una, ti.ti_ack) || SEQ_GT(ti.ti_ack, tp->snd_max)){
			goto dropwithreset;
		}
		// server side 3-way handshake complete
		listening_sock->pending_connection_buf.push(this);

		source_xid = this->src_path.xid(this->src_path.destination_node());
		destination_xid = this->dst_path.xid(this->dst_path.destination_node());
		xid_pair.set_src(source_xid);
		xid_pair.set_dst(destination_xid);
		// Map the src & dst XID pair to source port
		get_transport() -> XIDpairToSock.set(xid_pair, this);
		// push this socket into pending_connection_buf and let Xaccept handle that

		// finish the connection handshake
		get_transport() -> XIDpairToConnectPending.erase(key);
		get_transport() -> ChangeState(this, CONNECTED);

		// If the app is ready for a new connection, alert it
		if (!listening_sock->pendingAccepts.empty()) {
			xia::XSocketMsg *acceptXSM = listening_sock->pendingAccepts.front();
			get_transport() -> ReturnResult(listening_sock->port, acceptXSM);
			listening_sock->pendingAccepts.pop();
			delete acceptXSM;
		}
		if (listening_sock->polling){
			// tell API we are writeable
			get_transport()->ProcessPollEvent(listening_sock->get_id(), POLLIN|POLLOUT);
		}

		tcp_set_state(TCPS_ESTABLISHED);
		if ((tp->t_flags & (TF_RCVD_SCALE | TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE | TF_REQ_SCALE)){
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		tp->snd_wl1 = ti.ti_seq -1 ;

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < ti->ti_ack <= tp->snd_max
	 * then advance tp->snd_una to ti->ti_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	/* 815 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:

		// ack duplicate synacks to prevent deadlock when ack is lost
		// and client has no data to send (e.g. XfetchChunk)
		if (ti.ti_flags & XTH_SYN && ti.ti_flags & XTH_ACK && \
			ti.ti_ack == tp->snd_una && ti.ti_seq == tp->rcv_nxt){
			goto dropafterack;
		}

		// note: not considering fin packets as duplicate acks prevents
		// a fsm deadlock when both parties close the connection at the same time
		if (SEQ_LEQ(ti.ti_ack, tp->snd_una) && !(ti.ti_flags & XTH_FIN)){
			if (ti.ti_len == 0 && tiwin == tp->snd_wnd){
				get_transport()->_tcpstat.tcps_rcvdupack++;
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver)
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 */
				if ( tp->t_timer[TCPT_REXMT] == 0 || ti.ti_ack != tp->snd_una){
					tp->t_dupacks = 0;

				} else if (++tp->t_dupacks == TCP_REXMT_THRESH || (tp->t_dupacks > (TCP_REXMT_THRESH) && isPowerOfTwo(tp->t_dupacks))){

					tcp_seq_t onxt = tp->snd_nxt;
					u_int win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
					if (win < 4)
						win = 4;
					tp->snd_ssthresh = win * tp->t_maxseg;
					tp->t_timer[TCPT_REXMT] = 0; // clear the rexmt timer
					tp->t_rtt = 0;
					tp->snd_nxt = ti.ti_ack;
					tp->snd_cwnd = tp->snd_ssthresh; // fast recovery!
					//debug_output(VERB_TCP, "[%s] now: [%u] cwnd: %u, 3 dups, slowstart", SPKRNAME, get_transport()->tcp_now(), tp->snd_cwnd);

					tcp_output();
					tp->snd_cwnd = tp->snd_ssthresh + tp->t_maxseg *
						tp->t_dupacks;
					//debug_output(VERB_TCP, "[%s] now: [%u] cwnd: %u, 3 dups, slowstart", SPKRNAME, get_transport()->tcp_now(), tp->snd_cwnd );

					if (SEQ_GT(onxt, tp->snd_nxt)){
						tp->snd_nxt = onxt; // starts sending later packets again!
					}
					goto drop;

				} else if (tp->t_dupacks > TCP_REXMT_THRESH){
					tp->snd_cwnd += tp->t_maxseg;
					//debug_output(VERB_TCP, "[%s] now: [%u] cwnd: %u, dups", SPKRNAME, get_transport()->tcp_now(), tp->snd_cwnd );
					tcp_output();
					goto drop;
				}
			} else{
				tp->t_dupacks = 0;
			}
			break;
		}

		/* 888 */
		if (tp->t_dupacks > TCP_REXMT_THRESH &&
			tp->snd_cwnd > tp->snd_ssthresh){
			tp->snd_cwnd = tp->snd_ssthresh;

			//debug_output(VERB_TCP, "%u: cwnd: %u, reduced to ssthresh", get_transport()->tcp_now(), tp->snd_cwnd );
		}
		tp->t_dupacks = 0;

		if (SEQ_GT(ti.ti_ack, tp->snd_max)){
			get_transport()->_tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack;
		}
		acked = ti.ti_ack - tp->snd_una;
		get_transport()->_tcpstat.tcps_rcvackpack++;
		get_transport()->_tcpstat.tcps_rcvackbyte += acked;

		/* 903 */

		//debug_output(VERB_TCP, "[%s] now: [%u]  RTT measurement: ts_present: %u, now: %u, ecr: %u", SPKRNAME, get_transport()->tcp_now(), ts_present, get_transport()->tcp_now(), ts_ecr);

		if (ts_present){
			tcp_xmit_timer(get_transport()->tcp_now() - ts_ecr + 1);
		} else if (tp->t_rtt && SEQ_GT(ti.ti_ack, tp->t_rtseq)){
			tcp_xmit_timer( tp->t_rtt );
		}

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (ti.ti_ack == tp->snd_max){
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0){
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
		}

		/* 927 */
		{
		u_int cw = tp->snd_cwnd;
		u_int incr = tp->t_maxseg;
		if (cw > tp->snd_ssthresh ){
			incr = incr * incr / cw + incr / 8;
		}
		tp->snd_cwnd = min(cw + incr, (u_int) TCP_MAXWIN << tp->snd_scale);
		//debug_output(VERB_TCP, "[%s] now: [%u] cwnd: %u, increase: %u", SPKRNAME, get_transport()->tcp_now(), tp->snd_cwnd, incr);
		}

		/* 943 */
		//NOTICE: unsigned/signed comparison acked is an int, byte_length() returns an unsigned 32 int
		//this is taken verbatim from TCP Illustrated vol2
		if (acked > (int)_q_usr_input.byte_length()){
			tp->snd_wnd -= _q_usr_input.byte_length();
			_q_usr_input.drop_until(_q_usr_input.byte_length());
			ourfinisacked = true;
		} else{
			_q_usr_input.drop_until(acked);
			tp->snd_wnd -= acked;
			ourfinisacked = false;
		}
		if (_staged){ // because drop_until frees up queue space
			unstage_data();
		}

		tp->snd_una = ti.ti_ack;

		if (SEQ_LT(tp->snd_nxt, tp->snd_una)){
			tp->snd_nxt = tp->snd_una;
		}

		/* 957 */
		switch (tp->t_state){
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked){
				tp->t_timer[TCPT_2MSL] = get_transport()->globals()->tcp_maxidle;
				tcp_set_state(TCPS_FIN_WAIT_2);
			}
			break;
			/* 985 */
		case TCPS_CLOSING:
			if (ourfinisacked){
				tcp_set_state(TCPS_TIME_WAIT);
				tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;

				assert(pktlen == ti.ti_len);
				if (_q_recv.push(pkt, pktlen, ti.ti_seq, ti.ti_seq + ti.ti_len) < 0){
					//debug_output(VERB_ERRORS, "TCPClosing segment push into reassembly Queue FAILED");
					if (pkt != NULL) {
						free(pkt);
						pkt = NULL;
					}
				} else if (!_q_recv.is_empty() && has_pullable_data()){
					tp->rcv_nxt = _q_recv.last_nxt();
					check_for_and_handle_pending_recv();
					//  debug_output(VERB_TCPSTATS, "input (closing) updating rcv_nxt to [%u]", tp->rcv_nxt);
				}
			}
			break;
			/* 993 */
		case TCPS_LAST_ACK:
			if (ourfinisacked){
				tcp_set_state(TCPS_CLOSED);
				goto drop;
			}
			break;
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			goto dropafterack;
		}
	}

	/* 1015 Update the Send Window */
step6:
	// DRB - this code from the book make the compiler go crazy
	// I think I have the ()'s doing the right precedence so that it's happy now

	/* Check for ACK flag AND any one of the following 3 conditions */
	if ((tiflags & XTH_ACK) &&
		(
			SEQ_LT(tp->snd_wl1, ti.ti_seq) ||
			(tp->snd_wl1 == ti.ti_seq && (SEQ_LT(tp->snd_wl2, ti.ti_ack))) ||
			(tp->snd_wl2 == ti.ti_ack && tiwin > tp->snd_wnd)
		)
	){

		/* Keep track of pure window updates */
		if (ti.ti_len == 0 && tp->snd_wl2 == ti.ti_ack && tiwin > tp->snd_wnd){
			get_transport()->_tcpstat.tcps_rcvwinupd++;
		}

		tp->snd_wnd = tiwin;
		tp->snd_wl1 = ti.ti_seq;
		tp->snd_wl2 = ti.ti_ack;
		if (tp->snd_wnd > tp->max_sndwnd){
			tp->max_sndwnd = tp->snd_wnd;
		}
		//tp->t_flags |= TF_ACKNOW; //REMOVE THIS it should not be here - added by dan as a test to make tcp_output send ack when we get out-of-order segment
		needoutput = 1;
	}

	 /*1094*/
	// either a data packet or a fin
	if ((ti.ti_len || (tiflags & XTH_FIN)) &&
		TCPS_HAVERCVDFIN(tp->t_state) == 0){

		//printf("becareful 843\n");
		/* begin TCP_REASS */
		if (ti.ti_seq == tp->rcv_nxt &&
			tp->t_state == TCPS_ESTABLISHED){
				tp->t_flags |= TF_DELACK;
				tp->rcv_nxt += ti.ti_len;
				tiflags = ti.ti_flags & XTH_FIN;
		}

		//Dan's experimental ACK_NOW: if ti.ti_seq > tp->rcv_nxt, acknow
		if (ti.ti_seq > tp->rcv_nxt && tp->t_state == TCPS_ESTABLISHED){
			tp->t_flags |= TF_ACKNOW;
		}

		/* _q_recv.push() corresponds to the tcp_reass function whose purpose is
		 * to put all data into the TCPQueue for both possible reassembly and
		 * in-order presentation to the "application socket" which in the
		 * TCPget_transport is the stateless pull port.
		 */

		// if this is a duplicate segment, push will return a negative value
		assert(pktlen == ti.ti_len);
		if (_q_recv.push(pkt, pktlen, ti.ti_seq, ti.ti_seq + ti.ti_len) < 0){

			//debug_output(VERB_ERRORS, "TCPClosing segment push into reassembly Queue FAILED");
			if (pkt) {
				free(pkt);
				pkt = NULL;
			}
		} else if (! _q_recv.is_empty() && has_pullable_data()){

			tp->rcv_nxt = _q_recv.last_nxt();
			check_for_and_handle_pending_recv();
			//debug_output(VERB_TCPSTATS, "input (sp) updating rcv_nxt to [%u]", tp->rcv_nxt);
		}
		/* end TCP_REASS */

		// TODO len calculation @Harald: What exactly needs to be done?
		//len = ti.ti_len;
	} else{
		if (pkt != NULL){
			free(pkt);
			pkt = NULL;
		}
		// p->kill();		// don't know why comment this in order to prevent deadlock
		tiflags &= ~XTH_FIN;
	}

	/*1116*/
	/* FIN processing */
	if ( tiflags & XTH_FIN ){
		// tell API peer requested close
		if (isBlocking){
			if (recv_pending){
				// The api is blocking on a recv, return 0 bytes available
				get_transport() -> ReturnResult(port, pending_recv_msg, 0, 0);
				recv_pending = false;
				delete pending_recv_msg;
				pending_recv_msg = NULL;
			}
		}
		if (polling){
			get_transport()->ProcessPollEvent(get_id(), POLLIN|POLLHUP);
		}

		if (TCPS_HAVERCVDFIN(tp->t_state) == 0){
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state){
		case TCPS_SYN_RECEIVED:
		case TCPS_ESTABLISHED:
			if ( tp->so_flags & SO_FIN_AFTER_TCP_FIN ){
				tcp_set_state(TCPS_LAST_ACK);
			} else{
				tcp_set_state(TCPS_CLOSE_WAIT);
			}
			break;
		case TCPS_FIN_WAIT_1:
			tcp_set_state(TCPS_CLOSING);
			break;
		case TCPS_FIN_WAIT_2:
			tcp_set_state(TCPS_TIME_WAIT);
			tcp_canceltimers();
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			// the socket is really disconnected
			break;
		case TCPS_TIME_WAIT:
			//debug_output(VERB_TCP, "%u: TIME_WAIT", get_transport()->tcp_now());
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			break;
		}
	}

	/*1163*/
	if (needoutput || (tp->t_flags & TF_ACKNOW)){
		//debug_output(VERB_TCPSTATS, "[%s] we need output! true?: [%x] needoutput: [%x]", SPKRNAME, (tp->t_flags & TF_ACKNOW), needoutput);
		tcp_output();
	}

	return;

dropafterack:
	/* Drop incoming segment and send an ACK. */
	if (tiflags & XTH_RST){
		goto drop;
	}

	//printf("becareful 913\n");
	if (pkt != NULL){
		free(pkt);
		pkt = NULL;
	}
	if (p != NULL){
		p->kill();
	}
	tp->t_flags |= TF_ACKNOW;
	tcp_output();

	return;


dropwithreset:
	/*
	 * Generate a RST and drop incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if (tiflags & XTH_ACK){
		tcp_respond((tcp_seq_t)0+1, ti.ti_ack, XTH_RST);
	} else{
		if (tiflags & TH_SYN){
			ti.ti_len++;
		}
		tcp_respond(ti.ti_seq+ti.ti_len+1, (tcp_seq_t)0, XTH_RST|XTH_ACK);
	}
	return;

drop:
	//debug_output(VERB_TCP, "[%s] tcpcon::input drop", SPKRNAME);
	if (pkt != NULL){
		free(pkt);
		pkt = NULL;
	}
	if (p != NULL){
		p->kill();
	}
	return;
}


/* Send data from the TCP FIFO in the stateful way out to the tcp-speaking
 * client.
 */
void XStream::tcp_output(){

	int 		idle, sendalot, off, flags;
	unsigned 	optlen, hdrlen;
	u_char		opt[MAX_TCPOPTLEN];
	long		len, win;
	xtcp 	ti;
	WritablePacket *p = NULL;

	// Build XIA header so it's length is known for pkt size calculation
	XIAHeaderEncap xiaHdr;
	xiaHdr.set_nxt(CLICK_XIA_NXT_XSTREAM);
	xiaHdr.set_hlim(hlim);
	xiaHdr.set_dst_path(dst_path);
	xiaHdr.set_src_path(src_path);

	ti.th_nxt = CLICK_XIA_NXT_DATA;

	memset(opt, 0, MAX_TCPOPTLEN);   // clear them options

	/*61*/
	idle = (tp->snd_max == tp->snd_una);
	if (idle && tp->t_idle >= tp->t_rxtcur){
		tp->snd_cwnd = tp->t_maxseg;

	   //debug_output(VERB_TCP, "[%s] now: [%u] cnwd: %u, been idle", SPKRNAME, get_transport()->tcp_now());
	}

	// TCP FIFO (Addresses (and seq num) decrease in this dir ->)
	//							   snd_nxt  (off)  snd_una
	//		   ----------------------------------------
	// push -> | empty  | unsent data | sent, unacked | -> pop
	//		   ----------------------------------------
	//								  {	  off	  }
	// 		   			{  _q_usr_input.byte_length() }

	sendalot = 0;
	/*71*/
	/* off is the offset in bytes from the beginning of the send buf of the
	 * first data byte to send - a.k.a. bytes already sent, but unacked*/
	off = tp->snd_nxt - tp->snd_una;
	win = min(tp->snd_wnd, tp->snd_cwnd);
	flags = tcp_outflags[tp->t_state];
	/*80*/
	if (tp->t_force){
		if (win == 0){
			if (!_q_usr_input.is_empty()){
				flags &= ~XTH_FIN;
			}
			win = 1;
		} else{
			tp->t_timer[TCPT_PERSIST] = 0;
			tp->t_rxtshift = 0;
		}
	}
	/* we subtract off, because off bytes have been sent and are awaiting
	 * acknowledgement */
	len = min(_q_usr_input.byte_length(),  win) - off;

	/*106*/
	if (len < 0){
		len = 0;
		if (win == 0){
			tp->t_timer[TCPT_REXMT] = 0;
			tp->snd_nxt = tp->snd_una;
		}
	}

	if (_q_usr_input.pkts_to_send(off, win) > 1) { sendalot = 1; }
	if (len > tp->t_maxseg) { len = tp->t_maxseg; }

	win = so_recv_buffer_space();

	// don't send FIN yet if we still have data queued
	if (! _q_usr_input.is_empty()){
		flags &= ~TH_FIN;
	}

	/*213*/
	if ((!_q_usr_input.is_empty()) && tp->t_timer[TCPT_REXMT] == 0 &&
		tp->t_timer[TCPT_PERSIST] == 0){
		tp->t_rxtshift = 0;
		tcp_setpersist();
	}

	/*131 Silly window avoidance */
	if (len){
		if (len == tp->t_maxseg){
			goto send;
		}
		if ((idle || tp->t_flags & TF_NODELAY) &&
			len + off >= _q_usr_input.byte_length()){
			goto send;
		}
		if (tp->t_force){
			goto send;
		}
		if ((unsigned)len >= tp->max_sndwnd / 2){
			goto send;
		}
		if (SEQ_LT(tp->snd_nxt, tp->snd_max)){
			goto send;
		}
	}

	/*154*/
	if (win > 0){
		long adv = min(win, (long)TCP_MAXWIN << tp->rcv_scale) -
		(tp->rcv_adv - tp->rcv_nxt);
		//debug_output(VERB_TCPSTATS, "[%s] adv: [%d] = min([%u],[%u]):  - (radv: [%u] rnxt: [%u]) [%u]", SPKRNAME, adv, win, (long)TCP_MAXWIN << tp->rcv_scale, tp->rcv_adv, tp->rcv_nxt, tp->rcv_adv-tp->rcv_nxt);
		/* Slight Hack Below - we are using (t_maxseg + 1) here to ensure that
		 * once we have recvd at least 1 byte more than a full MSS we goto send
		 * to dispatch an ACK to the sender. This is necessary because the
		 * incoming tcp payload bytes are less than maxseg due to header options. */
		if (adv >= (long) (tp->t_maxseg + 1)){
			goto send;
		}
		if (2 * adv >= TCP_MAXWIN){
			goto send;
		}
	} else{
		//printf("\t\t\t\twin <= 0!!!\n");
		//debug_output(VERB_TCPSTATS, "[%s] win: [%u]  (radv: [%u] rnxt: [%u]) [%u]", SPKRNAME, win, tp->rcv_adv, tp->rcv_nxt, tp->rcv_adv-tp->rcv_nxt);
	}

	/*174*/
	if (tp->t_flags & TF_ACKNOW){
		goto send;
	}
	if (flags & ( XTH_SYN | XTH_RST )){
		goto send;
	}
	if (SEQ_GT(tp->snd_up, tp->snd_una)){
		goto send;
	}

	if (flags & XTH_FIN &&
		((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una)){
		goto send;
	}

	// XIA active session migration
	// TODO: skip migration if not in established state
	if (migrating or migrateacking){
		goto send;
	}

	return; // didn't send anything

	/*222*/
send:
	optlen = 0;
	hdrlen = sizeof(xtcp);

	// a SYN or SYN/ACK flagged segment is to be created
	if (flags & XTH_SYN){
		tp->snd_nxt = tp->iss;
		if ((tp->t_flags & TF_NOOPT) == 0){
			opt[0] = TCPOPT_MAXSEG;
			opt[1] = TCPOLEN_MAXSEG >> 2; // pack len in multiples of 4 bytes
			const uint16_t mss = htons((uint16_t)tcp_mss(0));
			memcpy( &(opt[2]), &mss, 2);
			optlen = 4;

			// Here we set the Window Scale option if it was requested of us to
			// do so
			//debug_output(VERB_DEBUG, "[%s] t_flags: [%x]", SPKRNAME, tp->t_flags);
			if ((tp->t_flags & TF_REQ_SCALE) &&
				((flags & XTH_ACK) == 0 ||
				 (tp->t_flags & TF_RCVD_SCALE))){
				*((uint32_t *) (opt + optlen) ) = htonl( //FIXME 4-byte ALIGNMENT problem occurs here
					TCPOPT_WSCALE << 24 |
					(TCPOLEN_WSCALE >> 2) << 16 |
					(u_short)tp->request_r_scale); // len is 4-byte multiple now
				optlen += 4;
			}
		}
	}
	/* 253 timestamp generation */
//	//debug_output(VERB_DEBUG, "[%s] timestamp: [%X] [%x] [%x] [%x]", SPKRNAME, (tp->t_flags),((flags & TH_RST) == 0), ((flags & (TH_SYN | TH_ACK)) == TH_SYN),(tp->t_flags & TF_RCVD_TSTMP));

	//HACK FIX TO MAKE TIMESTAMPS GET Printed as the first stmt evalutates false when window scaling is > 0
	if (((tp->t_flags & (TF_REQ_TSTMP | TF_NOOPT)) == TF_REQ_TSTMP || 1 ) &&
	(flags & XTH_RST) == 0 &&
	((flags & (XTH_SYN | XTH_ACK)) == XTH_SYN ||
	(tp->t_flags & TF_RCVD_TSTMP))){
		//debug_output(VERB_DEBUG, "[%s] timestamp: SETTING TIMESTAMP", SPKRNAME);
		uint32_t *lp = (uint32_t*) (opt + optlen);
		*lp++ = htonl(TCPOPT_TSTAMP_HDR); // first 2 bytes of option are 0
		*lp++ = htonl(get_transport()->tcp_now());
		*lp = htonl(tp->ts_recent);
		optlen += TCPOLEN_TIMESTAMP;
	} else{
		// Remove this clause after it's been debugged and timestamps are working properly
		//debug_output(VERB_DEBUG, "[%s] timestamp: NOT setting timestamp", SPKRNAME);
	}

	// Include the MIGRATE option if the migrating flag is set
	// TODO: Ensure we are not exceeding max packet size with all options
	if (migrating){

		if (tp->t_timer[TCPT_REXMT] == 0 && tp->t_timer[TCPT_PERSIST] == 0){
			tp->t_rxtshift = 0;
			tcp_setpersist();
		}

		int migratelen, padlen;

		u_char *migrateoptptr = opt + optlen;

		// Mark the option as a MIGRATE
		*migrateoptptr = TCPOPT_MIGRATE;

		// Build a migrate message
		XIASecurityBuffer migrate_msg(900);
		if (build_migrate_message(migrate_msg, src_path, dst_path, last_migrate_ts)
				== false){
			click_chatter("ERROR: Failed to build MIGRATE message");
			migrating = false;
			return; // didn't send anything
		}

		// option size is migrate length + two bytes for kind and option len
		// plus, pad with zeroes at end to make a multiple of 4
		padlen = 4 - ((migrate_msg.size() + 2) % 4);
		migratelen = migrate_msg.size() + 2 + padlen;
		assert(migratelen % 4 == 0);

		// Ensure that XIASecurityBuffer won't blow up with extra bytes at end
		// Didn't look like it would
		// Put length >> 2 of option at *(migrateoptptr+1)
		*(migrateoptptr + 1) = migratelen >> 2;

		// Put migrate message at *(migrateoptptr+2)
		memcpy(migrateoptptr+2, migrate_msg.get_buffer(), migrate_msg.size());

		optlen += migratelen;
		assert(optlen <= MAX_TCPOPTLEN);

		click_chatter("Tx migrate msg for sock id %u len %d", this->id, optlen-2);
	}

	// Include the MIGRATEACK option if the migrateacking flag is set
	if (migrateacking){

		int migrateacklen, padlen;

		u_char *migrateackoptptr = opt + optlen;

		// Mark the option as a MIGRATEACK
		*migrateackoptptr = TCPOPT_MIGRATEACK;

		// Build a migrateack message
		XIASecurityBuffer migrate_ack(900);
		if (build_migrateack_message(migrate_ack, src_path, dst_path,
					last_migrate_ts) == false){
			click_chatter("ERROR: Failed to build MIGRATEACK message");
			migrateacking = false;
			return; // didn't send anything
		}

		padlen = 4 - ((migrate_ack.size() + 2) % 4);
		migrateacklen = migrate_ack.size() + 2 + padlen;
		assert(migrateacklen % 4 == 0);

		*(migrateackoptptr + 1) = migrateacklen >> 2;

		memcpy(migrateackoptptr+2, migrate_ack.get_buffer(), migrate_ack.size());

		// We don't retransmit MIGRATEACK messages, so set flag to false
		migrateacking = false;
		optlen += migrateacklen;

		assert(optlen <= MAX_TCPOPTLEN);
		click_chatter("Tx migrateack for sock id %u len %d", this->id, optlen-2);
	}
	hdrlen += optlen;

	size_t overhead = xiaHdr.hdr_size() + sizeof(ti) + optlen;
	if (len > (long)(tp->t_maxseg - overhead)){
		len = tp->t_maxseg - overhead;
		assert(len > 0);
		sendalot = 1;
	}

	/*278*/
	if (len){
		if (_staged){ // because drop_until frees up queue space
			unstage_data();
		}

		p = _q_usr_input.get(off, len);

		if (!p){
			//debug_output(VERB_ERRORS, "[%s] offset [%u] not in fifo!", SPKRNAME, off);
			return; // didn't send anything
		}
		if (p->length() > len){
			p->take(p->length() - len);
		}
		if (p->length() < len){
			len = p->length();
			sendalot = 1;
		}
		// p = p->push( sizeof(click_ip) + sizeof(xtcp) + optlen);

	/*317*/
	} else{
		// empty packet
		// p = Packet::make(sizeof(click_ip) + sizeof(xtcp) + optlen);
		/* TODO: errorhandling */
	}

	/*339*/
	if (flags & XTH_FIN && tp->t_flags & TF_SENTFIN &&
		tp->snd_nxt == tp->snd_max){
		tp->snd_nxt --;
	}

	// @Harald: Is there a reason that the persist timer was not being checked?
	if (len || (flags & (XTH_SYN | XTH_FIN)) || tp->t_timer[TCPT_PERSIST]){
		ti.th_seq = htonl(tp->snd_nxt);
	} else{
		ti.th_seq = htonl(tp->snd_max);
	}

	ti.th_ack = htonl(tp->rcv_nxt);
	if (optlen){
		//memcpy(reinterpret_cast<uint8_t*>(&ti) + sizeof(xtcp), opt, optlen);
		ti.th_off = (sizeof(xtcp) + optlen) >> 2;
	}

	ti.th_flags = htons(flags);

	/*370*/
	/* receiver window calculations */

	/*TODO: silly window */
	// Correct window if it is too large or too small
	if (win > (long) TCP_MAXWIN << tp->rcv_scale){
		win = (long) TCP_MAXWIN << tp->rcv_scale;
	}
	if (win < (long) (tp->rcv_adv - tp->rcv_nxt)){
		win = (long) (tp->rcv_adv - tp->rcv_nxt);
	}

	// Set the tcp header window size we will advertisement
	ti.th_win = htonl((u_short) (win >> tp->rcv_scale));
	tp->snd_up = tp->snd_una;

	/* TODO: do we need to set p->length here ??? */

	/*400*/
	if (tp->t_force == 0  || tp->t_timer[TCPT_PERSIST] == 0){
		tcp_seq_t startseq = tp->snd_nxt;

		if (flags & (XTH_SYN | XTH_FIN)){
			if (flags & XTH_SYN){
				tp->snd_nxt++;
			}
			if (flags & XTH_FIN){
				tp->snd_nxt++;
				tp->t_flags |= TF_SENTFIN;
			}
		}

		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)){
			tp->snd_max = tp->snd_nxt ;
			if (tp->t_rtt == 0){
				tp->t_rtt = 1;
				tp->t_rtseq = startseq;
			}
		}

		if (tp->t_timer[TCPT_REXMT] == 0 && tp->snd_nxt != tp->snd_una){
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
			//debug_output(VERB_TCP, "[%s] now: [%u] REXMT set to %u == %f", SPKRNAME, get_transport()->tcp_now(), tp->t_timer[TCPT_REXMT], tp->t_timer[TCPT_REXMT]*0.5 );
			if (tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}

	} else if (SEQ_GT(tp->snd_nxt + len, tp->snd_max)){
		tp->snd_max = tp->snd_nxt + len;
	}

	// THE MAGIC MOMENT! Our beloved tcp data segment goes to be wrapped in IP and
	// sent to its tcp-speaking destination :-)

	// if there is no data to send, create an empty packet
	if (p == NULL){
		p = WritablePacket::make((uint32_t) 1512 /*headroom*/, NULL /*data*/, \
			0 /*length*/, 0 /*tailroom*/);
	}

	// add stream header
	StreamHeaderEncap *streamHdr = \
		StreamHeaderEncap::MakeTCPHeader(&ti, opt, optlen);
	p = streamHdr->encap(p);
	delete streamHdr; // no longer needed

	// add network header
	p = xiaHdr.encap(p, true /*adjust_plen*/);

	if (win > 0 && SEQ_GT(tp->rcv_nxt + win, tp->rcv_adv)) {
		tp->rcv_adv = tp->rcv_nxt + win;
	}
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW | TF_DELACK);

	get_transport()->output(NETWORK_PORT).push(p);

	/* Data has been sent out at this point. If we advertised a positive window
	 * and if this new window advertisement will result in us recieving a higher
	 * sequence numbered segment than before this window announcement, we record
	 * the new highest sequence number which the sener is allowed to send to us.
	 * (tp->rcv_adv). Any pending ACK has now been sent. */

	if (sendalot){
		_outputTask.reschedule(); // reschedule to send another packet.
						// but don't send immediately because
						// there may be some acks we need to
						// process first!
	}

	return; // sent something!
}

void
XStream::tcp_respond(tcp_seq_t ack, tcp_seq_t seq, int flags)
{
	xtcp th;

	int win = min(so_recv_buffer_space(), (tcp_seq_t)(TCP_MAXWIN << tp->rcv_scale));

	if (! (flags & XTH_RST)){
		flags = XTH_ACK;
		th.th_win = htonl((u_short)(win >> tp->rcv_scale));

	} else {
		th.th_win = htonl((u_short)win);
	}

	th.th_nxt = CLICK_XIA_NXT_DATA;
	th.th_seq =   htonl(seq);
	th.th_ack =   htonl(ack);
	th.th_flags = htons(flags);
	th.th_off = sizeof(xtcp) >> 2;

	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_nxt(CLICK_XIA_NXT_XSTREAM);
	xiah.set_hlim(hlim);
	xiah.set_dst_path(dst_path);
	xiah.set_src_path(src_path);

	StreamHeaderEncap *send_hdr = StreamHeaderEncap::MakeTCPHeader(&th);
	// sending a control packet
	WritablePacket *p =  WritablePacket::make((uint32_t)0, '\0', 0, 0);
	WritablePacket *tcp_payload = send_hdr->encap(p);
	xiah.set_plen(send_hdr->hlen()); // XIA payload = transport header + transport-layer data
	tcp_payload = xiah.encap(tcp_payload, false);
	delete send_hdr;
	get_transport()->output(NETWORK_PORT).push(tcp_payload);

}

tcp_seq_t
XStream::so_recv_buffer_space() {
	return so_recv_buffer_size - _q_recv.bytes_ok();
}


void
XStream::fasttimo() {

	if ( tp->t_flags & TF_DELACK) {
		tp->t_flags &= ~TF_DELACK;
		tp->t_flags |= TF_ACKNOW;
		tcp_output();
	}
}

void
XStream::slowtimo() {

	int i;
	//debug_output(VERB_TIMERS, "[%s] now: [%u] Timers: %s %d %s %d %s %d %s %d %s %d",
	// SPKRNAME,
	// get_transport()->tcp_now(),
	// tcptimers[0], tp->t_timer[0],
	// tcptimers[1], tp->t_timer[1],
	// tcptimers[2], tp->t_timer[2],
	// tcptimers[3], tp->t_timer[3],
	// tcptimers[4], tp->t_timer[4] );

	for (i=0; i < TCPT_NTIMERS; i++){
		// //debug_output(VERB_TCP, "%u: XStream::slowtimo: %s %d\n", get_transport()->tcp_now(), tcptimers[i], tp->t_timer[i]);
		if (tp->t_timer[i] && --(tp->t_timer[i]) == 0){
		  // StringAccum sa;
		  // sa << *(flowid());

		  //   //debug_output(VERB_TIMERS, "[%s] now: [%u] TIMEOUT %s: %s, now: %u", SPKRNAME, get_transport()->tcp_now(), sa.c_str(), tcptimers[i], get_transport()->tcp_now());
			tcp_timers(i);
		}
	}
	tp->t_idle++;
	if (tp->t_rtt){
		tp->t_rtt++;
	}
}

static int tcp_backoff[TCP_MAXRXTSHIFT] = {1, 1, 2, 4, 8, 16, 32, 64, 128, \
  256, 512, 1024, 1024, 1024, 1024, 2048, 2048, 2048, 2048, 2048, 2048, 2048, \
  4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096};

void
XStream::tcp_timers (int timer) {
	int rexmt;

	switch (timer){
		/*127*/
		case TCPT_2MSL:
		  if (tp->t_state != TCPS_TIME_WAIT &&
			  tp->t_idle <= get_transport()->globals()->tcp_maxidle){
			tp->t_timer[TCPT_2MSL] = get_transport()->globals()->tcp_keepintvl;
		  } else{
			tcp_set_state(TCPS_CLOSED);
			// the socket is really closed
		  }
		  break;
		case TCPT_PERSIST:
		  tcp_setpersist();
		  tp->t_force = 1;
		  tcp_output();
		  tp->t_force = 0;
		  break;

		case TCPT_KEEP:

		  if (tp->t_state < TCPS_ESTABLISHED){
			get_transport()->_tcpstat.tcps_keepdrops++;
			printf("tcp_drop #3\n");
			tcp_drop(ETIMEDOUT);
			break;

		  } else if((tp->t_state == TCPS_ESTABLISHED || \
				tp->t_state == TCPS_CLOSE_WAIT) && \
				tp->so_flags & SO_KEEPALIVE){

			if (tp->t_idle >= get_transport()->globals()->tcp_keepidle + \
				get_transport()->globals()->tcp_maxidle){

				get_transport()->_tcpstat.tcps_keepdrops++;
				printf("tcp_drop #4\n");
				tcp_drop(ETIMEDOUT);
				break;
			}

			get_transport()->_tcpstat.tcps_keepprobe++;
			tcp_respond(tp->rcv_nxt+1, tp->snd_una, 0);
			tp->t_timer[TCPT_KEEP] = get_transport()->globals()->tcp_keepintvl;

		  } else{
			tp->t_timer[TCPT_KEEP] = get_transport()->globals()->tcp_keepidle;
		  }

		  break;
		case TCPT_REXMT:

		  if (tp->t_rxtshift >= TCP_MAXRXTSHIFT){ // retransmissions exhausted
			tp->t_rxtshift = TCP_MAXRXTSHIFT-1;
			printf("tcp_drop #5\n");
			tcp_drop(ETIMEDOUT);
			break;
		  }
		  rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
		  TCPT_RANGESET(tp->t_rxtcur, rexmt,
		  		tp->t_rttmin, TCPTV_REXMTMAX);
		  tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

		  if (tp->t_rxtshift == 5){ // meaning this is the sixth retransmission
			/* If we backed off this far, our srtt estimate is probably bogus.
			 * Clobber it so we'll take the next rtt measurement as our srtt;
			 * move the current srtt into rttvar to keep the current retransmit
			 * times until then.
			 *
			 * We could also notify the lower layer to try a different route, but
			 * that is not implemented at this point.
			 */
			tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
			tp->t_srtt = 0;
		  }

		  tp->t_rxtshift++; // increment rxtshift for next time

		  tp->snd_nxt = tp->snd_una;
		  tp->t_rtt = 0;

		  // fast recovery upon rexmt timeout, don't go back to slow start
		  // a change made by Rui
		  {
			u_int win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;

			if (win < 4){
				win = 4;
			}

			/*debug_output(VERB_TCP, "%u: cwnd: %u, TCPT_REXMT", \
				get_transport()->tcp_now(), tp->snd_cwnd);*/
			tp->snd_ssthresh = win * tp->t_maxseg;
			tp->snd_cwnd = tp->snd_ssthresh;
			tp->t_dupacks = 0;
		  }

		  tcp_output();
		  break;

		case TCPT_IDLE:
		  usrclosed();
		  break;
  }
}

void
XStream::tcp_canceltimers() {
	int i;
	for (i=0; i<TCPT_NTIMERS; i++)
		tp->t_timer[i] = 0;
}

void
XStream::tcp_setpersist() {
	int t;

	t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

	if (tp->t_timer[TCPT_REXMT]){
		_errh->error("tcp_output REXMT");
	}

	TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
		t * tcp_backoff[tp->t_rxtshift],
		TCPTV_PERSMIN, TCPTV_PERSMAX);
	if (tp->t_rxtshift < TCP_MAXRXTSHIFT-1){
		tp->t_rxtshift++;
	}
}
void
XStream::tcp_xmit_timer(short rtt) {
	short delta;
	get_transport()->_tcpstat.tcps_rttupdated++;

	//debug_output(VERB_TIMERS, "[%s] now: [%u]: tcp_xmit_timer: srtt [%d] cur rtt [%d]\n", SPKRNAME, get_transport()->tcp_now(), tp->t_srtt, rtt);
	if (tp->t_srtt != 0){
		/*
		 * srtt is stored as fixed point with 3 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = rtt - 1 - (tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0){
			tp->t_srtt = 1;
		}
		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 2 bits after the
		 * binary point (scaled by 4).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0){
			delta = -delta;
		}
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0){
			tp->t_rttvar = 1;
		}
	} else{
		/*
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << TCP_RTT_SHIFT;
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
	}
	tp->t_rtt = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
		tp->t_rttmin, TCPTV_REXMTMAX);
	//debug_output(VERB_TCP, "[%s] now: [%u]: rxt_cur: %u, RXMTVAL: %u, rttmin: %u, RXMTMAX: %u \n", SPKRNAME, get_transport()->tcp_now(), tp->t_rxtcur, TCP_REXMTVAL(tp), tp->t_rttmin,TCPTV_REXMTMAX );

	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	// tp->t_softerror = 0;
}

void
XStream::tcp_drop(int err)
{
	XSocketMsg xsm;

	// Notify API that the connection failed

	tp->so_error = so_error = err;

	if (isBlocking) {
		if (err == ECONNREFUSED) {
			xsm.set_type(xia::XCONNECT);
			xsm.set_sequence(0); // TODO: what should This be?
			xsm.set_id(get_id());
			xia::X_Connect_Msg *connect_msg = xsm.mutable_x_connect();
			connect_msg->set_status(xia::X_Connect_Msg::XFAILED);
			connect_msg->set_ddag(dst_path.unparse().c_str());
			get_transport()->ReturnResult(port, &xsm);
		} else {
			// FIXME: make sure we return errors up the stack when necessary!
			INFO("we should be telling the transport about this!\n");
		}
	}

	if (polling){
		get_transport()->ProcessPollEvent(get_id(), POLLHUP);
	}
	tcp_set_state(TCPS_CLOSED);
	if (TCPS_HAVERCVDSYN(tp->t_state)){
		tcp_output();
	}
}

u_int
XStream::tcp_mss(u_int offer) {
	unsigned glbmaxseg = get_transport()->_tcp_globals.tcp_mssdflt;
	/* FIXME sensible mss function */
	u_int mss;
	if (offer){
		mss = min(glbmaxseg, offer);
	} else{
		mss = glbmaxseg;
	}
	tp->t_maxseg = mss;
	tp->snd_cwnd = mss;

	//debug_output(VERB_TCP, "[%s] now: [%u] cnwd: [%u] rcvd_offer: [%u] tcp_mss: [%u]", SPKRNAME, get_transport()->tcp_now(), tp->snd_cwnd, offer, tp->t_maxseg);

	return mss;
}

/* Recieves a stateless mesh packet, passess it to have its headers removed, and
 * then if the packet has a payload, pushes it into the FIFO to be pushed to the
 * stateful reciever, OR if any stateless flags were set, performs the
 * appropriate action.
 */
int
XStream::usrsend(WritablePacket *p)
{
	// Sanity Check: We should never recieve a packet after our tcp state is
	// beyond CLOSE_WAIT.
	if (tp->t_state > TCPS_CLOSE_WAIT){
		p->kill();
		return EPIPE;	// what's the right result in this case?
	}

	if (tp->so_flags & SO_FIN_AFTER_UDP_IDLE){
		//debug_output(VERB_TIMERS, "[%s] tcpcon::usrsend setting timer TCPT_IDLE to [%d]",
			// SPKRNAME, get_transport()->globals()->so_idletime);
		tp->t_timer[TCPT_IDLE] = get_transport()->globals()->so_idletime;
	}

	int retval = 0;

	// If we were closed or listening, we will have to send a SYN
	if ((tp->t_state == TCPS_CLOSED) || (tp->t_state == TCPS_LISTEN)){
		tcp_set_state(TCPS_SYN_SENT);
	}

	// if (tp->t_sl_flags == XTH_SYN) {
	// 	usropen();
	// }

	// if (tp->t_sl_flags == XTH_FIN) {
	// 	usrclosed();
	// }

	if (_staged) {
		unstage_data();
	}

	if (p){
		//printf("usrsend: Push into _q_usr_input\n");
		//int remaining = (int)p -> length();
		//char buf[512];
		//memset(buf, 0, 512);
		//printf("the remaining is %d\n", remaining);

		// WritablePacket *wp = NULL;
		// while (remaining > 0) {
		// 	int size = remaining > 512 ? 512 : remaining;
		// 	memcpy((void*)buf, (const void*)p->data(), size);
		// 	wp = WritablePacket::make((const void*)buf, size);
		// 	if (size == 512)
		// 		p -> pull(512);
		retval = _q_usr_input.push(p);
		// 	remaining -= 512;
		// 	printf("the remaining is %d\n", remaining);
		// }
	}

	//  These are the states where we expect to receive packets
	//	if ( (tp->t_state == TCPS_ESTABLISHED) || ( tp->t_state == TCPS_CLOSE_WAIT ))
	if (retval == 0){
		tcp_output();
	}

	return retval;
}

/* requesting a stream session migration to new address */
void XStream::usrmigrate(){

	last_migrate_ts = Timestamp::now().unparse();

	// if we are waiting to retransmit, do it now
	if (tp->t_timer[TCPT_REXMT] > 0){

		click_chatter("usrmigrate branch #1");

		tp->t_timer[TCPT_REXMT] = 0;
		tp->t_rxtshift = 0;
		// set srtt to zero but first pass it on to rttvar so we don't
		// retransmit too soon
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
		tcp_timers(TCPT_REXMT); // tcp_output() called inside

	} else { // no data to send

		click_chatter("usrmigrate branch #2");
		tcp_output();
	}

}

/* user request 424*/
void
XStream::usrclosed()
{
	switch (tp->t_state){
		case TCPS_CLOSED:
		case TCPS_LISTEN:
		case TCPS_SYN_SENT:
			tcp_set_state(TCPS_CLOSED);
			break;
		case TCPS_SYN_RECEIVED:
		case TCPS_ESTABLISHED:
			tcp_set_state(TCPS_FIN_WAIT_1);
			break;
		case TCPS_CLOSE_WAIT:
			tcp_set_state(TCPS_LAST_ACK);
			break;
	}
	tcp_output();
}


void
XStream::usropen()
{
	if (tp->iss == 0){
		tp->iss = _tcp_iss();
		//debug_output(VERB_ERRORS, "Setting initial sequence to [%d], because it was 0", tp->iss);
		// Setting a non-zero initial sequence number because I see some weird
		// problems in wireshark when initial seq is 0
	}
	_tcp_sendseqinit(tp);
	//debug_output(VERB_STATES,"[%s] usropen with state <%s>, initial seq num <%d> \n",
		// dispatcher()->name().c_str(), tcpstates[tp->t_state], tp->iss);
	if (tp->t_state == TCPS_CLOSED || tp->t_state == TCPS_LISTEN){
		// cout << "we are good\n";
		tcp_set_state(TCPS_SYN_SENT);
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
	}
	//cout << "we are good+1\n";
	tcp_output();
}

/*
inline void
XStream::initialize(const int port)
{
	dispatcher()->//debug_output(VERB_STATES,"[%s] initialize for port <%d>\n",
		dispatcher()->name().c_str(), port);
	if ( port == 1 )
   	usropen();
} */


void
XStream::set_state(const HandlerState new_state) {

	HandlerState old_state = get_state();

	sock::set_state(new_state);

	if ((old_state == CREATE) && new_state == (INITIALIZE)){
		usropen();
	}

	if ((new_state == SHUTDOWN) && tcp_state() <= TCPS_ESTABLISHED){
		usrclosed();
	}

}

void
XStream::_tcp_dooptions(const u_char *cp, int cnt, uint8_t th_flags,
	int * ts_present, uint32_t *ts_val, uint32_t *ts_ecr)
{
	uint16_t mss;
	int opt, optlen;
	optlen = 0;
	XIAPath new_dst_path;

	//printf("[%s] tcp_dooption cnt [%u]\n", "Xstream", cnt);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL) {
			//printf("opt EOL, skipping the rest\n");
			break;
		}
		if (opt == TCPOPT_NOP){
			optlen = 1;
		} else{
			if (cnt < 2){
				//printf("opt NOP\n");
				break;
			}
			optlen = cp[1] << 2; // length is multiple of 4 bytes on wire
			if (optlen < 4 || optlen > cnt ){
				//printf("b3, optlen: [%x] cnt: [%x]", optlen, cnt);
				break;
			}
		}
		//click_chatter("[Xstream] doopts: Entering options switch stmt, optlen [%x]", optlen);
		switch (opt) {
			case TCPOPT_MAXSEG:
				//printf("[%s] doopts: case MAXSEG","XStream");
				if (optlen != TCPOLEN_MAXSEG){
					//printf("[%s] doopts: optlen: [%x] maxseg: [%x]", "XStream", optlen, TCPOLEN_MAXSEG);
					continue;
				}
				if (!(th_flags & XTH_SYN)){
					//printf("[%s] tcp_dooption SYN flag not set", "XStream");
					continue;
				}
				memcpy((char*) &mss, (char*) cp + 2, sizeof(mss));
				////debug_output(VERB_DEBUG, "[%s] doopts: mss p0: [%x]", SPKRNAME, mss);
				mss = ntohs(mss);
				////debug_output(VERB_DEBUG, "[%s] doopts: mss p1: [%x]", SPKRNAME, mss);
				tcp_mss(mss);
//				tp->t_maxseg = ntohs((u_short)*((char*)cp + 2)); //BUG WHY are we
//				setting this twice? once here and once in the line above?!
//				//debug_output(VERB_DEBUG, "[%s] doopts: mss p2: [%x]", SPKRNAME, mss);
				//avila this is 5... something here is broken. statically
				//setting MSS whenever it dips below 800
//				if (tp->t_maxseg < 800 || tp->t_maxseg > 1460) {
//					tp->t_maxseg = 1400;
//					//debug_output(VERB_ERRORS, "doopts: Recieved MAXSEG value [%d], setting to 1400", tp->t_maxseg);
//				}
				break;

			case TCPOPT_TIMESTAMP:
				//printf("[%s] doopts: case TIMESTAMP", "XStream");
				if (optlen != TCPOLEN_TIMESTAMP){
					continue;
				}
				*ts_present = 1;
				// kind=*cp, len=*(cp+1)<<2, cp+2 and cp+3 are zeroed
				bcopy((char *)cp + 4, (char *)ts_val, sizeof(*ts_val)); //FIXME: Misaligned
				*ts_val = ntohl(*ts_val);
				bcopy((char *)cp + 8, (char *)ts_ecr, sizeof(*ts_ecr)); //FIXME: Misaligned
				*ts_ecr = ntohl(*ts_ecr);

				//debug_output(VERB_DEBUG, "[%s] doopts: ts_val [%u] ts_ecr [%u]", SPKRNAME, *ts_val, *ts_ecr);
				if (th_flags & XTH_SYN){
					//debug_output(VERB_DEBUG, "[%s] doopts: recvd a SYN timetamp, ENABLING TIMESTAMPS", SPKRNAME);
					tp->t_flags |= TF_RCVD_TSTMP;
					tp->ts_recent = *ts_val;
					tp->ts_recent_age = get_transport()->tcp_now();
				}
				break;
#ifdef UNDEF
			case TCPOPT_SACK_PERMITTED:
				//printf("[%s] doopts: case SACK", "XStream");
				if (optlen != TCPOLEN_SACK_PERMITTED){
					continue;
				}
				if (!(flags & TO_SYN)){
					continue;
				}
				if (!tcp_do_sack){
					continue;
				}
				to->to_flags |= TOF_SACKPERM;
				break;
				case TCPOPT_SACK:
				if (optlen <= 4 || (optlen - 4) % TCPOLEN_SACK != 0){
					continue;
				}
				if (flags & TO_SYN){
					continue;
				}
				to->to_flags |= TOF_SACK;
				to->to_nsacks = (optlen - 4) / TCPOLEN_SACK;
				to->to_sacks = cp + 2;
				tcpstat.tcps_sack_rcv_blocks++;
				break;
#endif
			case TCPOPT_WSCALE:
				//printf("[%s] doopts: case WSCALE", "XStream");
				if (optlen != TCPOLEN_WSCALE){
					continue;
				}
				if (!(th_flags & XTH_SYN)){
					continue;
				}
				tp->t_flags |=  TF_RCVD_SCALE;

				// kind=*cp, len=*(cp+1)<<2, cp[2] is zeroed, cp[3] is the scale
				tp->requested_s_scale = min(cp[ 3], TCP_MAX_WINSHIFT);
				//debug_output(VERB_DEBUG, "[%s] WSCALE set, flags [%x], req_s_sc [%x]\n", SPKRNAME,
				break;
			case TCPOPT_MIGRATE:
				{
					String migrate_ts;

					click_chatter("Rx migrate msg for sock id %u, len %d", \
						this->id, optlen-2);

					XIASecurityBuffer migrate((const char *)cp+2, optlen-2);
					if (!valid_migrate_message(migrate, dst_path, src_path,
								new_dst_path, migrate_ts)){
						click_chatter("ERROR: invalid migrate message");
						continue;
					}
					// Peer migrated and sent a valid notification
					last_migrate_ts = migrate_ts;
					dst_path = new_dst_path;
					migrateacking = true;

					// if we are waiting to retransmit, do it now
					if (tp->t_timer[TCPT_REXMT] > 0){

						click_chatter("dooptions migrate branch #1");

						tp->t_timer[TCPT_REXMT] = 0;
						tp->t_rxtshift = 0;
						// set srtt to zero but first pass it on to rttvar so we don't
						// retransmit too soon
						tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
						tp->t_srtt = 0;
						tcp_timers(TCPT_REXMT); // tcp_output() called inside

					} else { // no data to send

						click_chatter("dooptions migrate branch #2");

						tcp_output();
					}
					break;
				}
			case TCPOPT_MIGRATEACK:
				{
					click_chatter("Rx migrateack msg for sock id %u, len %d", \
						this->id, optlen-2);

					XIASecurityBuffer migrateack((const char *)cp+2, optlen-2);
					if (!valid_migrateack_message(migrateack, dst_path,
								src_path, last_migrate_ts)){
						click_chatter("ERROR: invalid migrateack message");
						continue;
					}

					// Peer ack is valid, stop migrate retransmit
					migrating = false;
					break;
				}
			default:
			continue;
		}
	}
	//debug_output(VERB_DEBUG, "[%s] doopts: finished", SPKRNAME);
}


void
XStream::print_state(StringAccum &sa)
{
	UNUSED(sa);
	// int i;
	// sa << tcpstates[tp->t_state] << "\n";

	// sa.snprintf(80, "| Seq	: snd_nxt: %u, snd_una: %u, (in-flight: %u)\n",
	// 	tp->snd_nxt, tp->snd_una, tp->snd_nxt - tp->snd_una);
	// sa.snprintf(80, "| Windows: rcv_adv: %u, rcv_wnd: %u, snd_cwnd: %u \n",
	// 	tp->rcv_adv, tp->rcv_wnd, tp->snd_cwnd);
	// sa.snprintf(80, "| Timing: t_srtt: %u, t_rttvar: %u, now: %u\n",
	// 	tp->t_srtt, tp->t_rttvar, get_transport()->tcp_now());

	// sa << ("| Timers: ");
	// for(i=0; i<TCPT_NTIMERS; i++)
	//	 sa.snprintf(32, "%s: %d ", tcptimers[i], tp->t_timer[i]);
	// sa << "\n";
}

/**
* @brief check to see if the app is waiting for this data; if so, return it now
*
* @param tcp_conn
*/
void XStream::check_for_and_handle_pending_recv() {

	if (recv_pending){
		int bytes_returned = read_from_recv_buf(pending_recv_msg);
		get_transport()->ReturnResult(port, pending_recv_msg, bytes_returned);
		recv_pending = false;
		delete pending_recv_msg;
		pending_recv_msg = NULL;
	}
	if (polling){
		// tell API we are readable
		get_transport()->ProcessPollEvent(get_id(), POLLIN);
	}
}

/**
* @brief Read received data from buffer.
*
* We'll use this same xia_socket_msg as the response to the API:
* 1) We fill in the data (from *only one* packet for DGRAM)
* 2) We fill in how many bytes we're returning
* 3) We fill in the sender's DAG (DGRAM only)
* 4) We clear out any buffered packets whose data we return to the app
*
* @param xia_socket_msg The Xrecv or Xrecvfrom message from the API
* @param tcp_conn The XStream struct for this connection
*
* @return  The number of bytes read from the buffer.
*/
int XStream::read_from_recv_buf(XSocketMsg *xia_socket_msg) {

	xia::X_Recv_Msg *x_recv_msg = xia_socket_msg->mutable_x_recv();
	int bytes_requested = x_recv_msg->bytes_requested();
	bool peek = x_recv_msg->flags() & MSG_PEEK;
	int bytes_returned = 0;
	int bytes_pulled = 0;
	int extra = 0;
	char *buf;

	// FIXME: add code to throttle max xfer size
	bytes_requested = min(bytes_requested, 60 * 1024);

	if (_tail_length != 0){
		buf = _tail;
		bytes_pulled = _tail_length;
	} else{
		buf = (char *)malloc(65*1024);
	}

	while (has_pullable_data()){
		if (bytes_pulled >= bytes_requested){
			break;
		}

		uint32_t plen;
		char *p = _q_recv.pull_front(&plen);
		size_t data_size = plen;
		memcpy((void*)(&buf[bytes_pulled]), (const void*)p, data_size);
		bytes_pulled += data_size;
		free(p);
	}

	bytes_returned = min(bytes_requested, bytes_pulled);
	x_recv_msg->set_payload(buf, bytes_returned);
	x_recv_msg->set_bytes_returned(bytes_returned);

	extra = bytes_pulled - bytes_returned;

	if (peek){
		// we need to save all of the data we pulled for next call
		extra = bytes_pulled;
	} else if (extra != 0){
		// we have too much data. save the tail for next call
		memmove(buf, &buf[bytes_returned], extra);
	}

	if (extra){
		// save the data
		_tail = buf;
		_tail_length = extra;
	} else{
		_tail = NULL;
		_tail_length = 0;
		free(buf);
	}
	return bytes_returned;
}

tcpcb *
XStream::tcp_newtcpcb()
{
	tcpcb *tp = new tcpcb();
	if (tp == NULL){
		return NULL;
	}

	bzero((char*)tp, sizeof(tcpcb));
	tp->t_maxseg = get_transport()->globals()->tcp_mssdflt;
	tp->t_flags  = TF_REQ_SCALE | TF_REQ_TSTMP;
	tp->t_srtt   = TCPTV_SRTTBASE;
	tp->t_rttvar = get_transport()->globals()->tcp_rttdflt << 2;
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur,
		((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1, TCPTV_MIN,
		TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;

	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;

	tp->rcv_wnd = so_recv_buffer_space();

	tp->so_flags = get_transport()->globals()->so_flags;
	if (get_transport()->globals()->window_scale){
		tp->t_flags &= TF_REQ_SCALE;
		tp->request_r_scale = get_transport()->globals()->window_scale;
	}
	if (get_transport()->globals()->use_timestamp){
		tp->t_flags &= TF_REQ_TSTMP;
	}
	return tp;
}


XStream::XStream(XTRANSPORT *transport, const unsigned short port, uint32_t id)
	: sock(transport, port, id, SOCK_STREAM), _q_recv(this),
		_q_usr_input(TCP_FIFO_SIZE), _outputTask(transport, id) {

	tp = tcp_newtcpcb();
	tp->t_state = TCPS_CLOSED;

	so_recv_buffer_size = get_transport()->globals()->so_recv_buffer_size;

	_so_state = 0;

	_staged = NULL;
	_staged_seq = 0;
	_tail = NULL;
	_tail_length = 0;

	/*
	if (tp->so_flags & SO_FIN_AFTER_IDLE) {
	tp->idle_timeout = new Timer(TCPget_transport::_tcp_timer_close, this);
	tp->idle_timeout->initialize(get_transport());
	tp->idle_timeout->schedule_after_msec(tp->idle_wait_ms);
	}

	tp->timewait_timer = new Timer(TCPget_transport::_tcp_timer_wait, this);
	tp->timewait_timer->initialize(get_transport());
	*/
  //   if (dispatcher()->dispatch_code(true, 1) ==
	 //	(MFD_DISPATCH_MFD_DIRECT | MFD_DISPATCH_PULL)) {
		// //debug_output(VERB_DISPATCH, "[%s].<%x> Creating _stateless_pull task",
		// 	dispatcher()->name().c_str(), this);
		// _stateless_pull = new Task(&pull_stateless_input, this);
		// _stateless_pull->initialize(dispatcher()->router(), true);
  //   }

	// StringAccum sa;
	// sa << *(flowid());
	// //debug_output(VERB_STATES, "[%s] new connection %s %s", SPKRNAME, sa.c_str(), tcpstates[tp->t_state]);

	// initialize the output task
	ScheduleInfo::initialize_task(transport, &_outputTask, false /* schedule */,
		ErrorHandler::default_handler());
}

bool XStream::run_task(Task*){

	this->tcp_output();

	return true;
}

bool XStream::stage_data(WritablePacket *p, unsigned seq)
{
	// seq is API packet seq # not tcp seq #
	if (_staged == NULL){
		_staged = p;
		_staged_seq = seq;
		return true;
	} else{
		return false;
	}
}

int XStream::unstage_data()
{
	unsigned len;
	// if there's data, try to put it into the queue
	if (_staged) {
		len = _staged->length();

		if (_q_usr_input.push(_staged) !=  0){
			return -1;
		}

		// tell the API we let it go into the buffer
		xia::XSocketMsg xsm;
		xsm.set_type(xia::XRESULT);
		xsm.set_sequence(_staged_seq);
		xsm.set_id(get_id());

		get_transport()->ReturnResult(port, &xsm, len);

		_staged = NULL;
		_staged_seq = 0;

		return len;

	} else{
		return -1;
	}
}


/* Code for the (reassembly) queues
 *
 *  TODO: (OPTIMIZATION) this is currently allocating and freeing one
 *  TCPQueueElt object per packet.  I have no idea whether an array and a
 *  freemap would be better.  The pure static ringbuffer code from the other
 *  queues doesn't help, since we need to be able to queue packets in random
 *  order.
 */
TCPQueue::TCPQueue(XStream *con)
{
	_con = con ;
	_q_first = _q_last = _q_tail = NULL;
}


TCPQueue::~TCPQueue() {}


/**
 * Return values:
 * -3 means it's a duplicate packet.
 */
int
TCPQueue::push(char* p, uint32_t plen, tcp_seq_t seq, tcp_seq_t seq_nxt)
{
	uint32_t pktlen = plen;

	// we need a proper packet, non-zero length please
	if (p == NULL or !SEQ_GT(seq_nxt, seq)){
		return -1;
	}

	TCPQueueElt *qe = NULL;
	TCPQueueElt *wrk = NULL;
	// StringAccum sa;

	////debug_output(VERB_TCPQUEUE, "TCPQueue:push pkt:%ubytes, seq:%ubytes", p->length(), seq_nxt-seq);

	/* TCP Queue (Addresses (and seq num) decrease in this dir ->)
	 *
	 *		   ---------------------------------------------
	 * push -> |   empty   | seg_c | seg_b |  gap  | seg_a | -> pull_front
	 *		   ---------------------------------------------
	 *				 	  {_q_tail}^   {_q_last = _q_first}^
	 *
	 * _q_first points to the pkt with the lowest seq num in the queue
	 * _q_last points to the pkt with the highest seq num where no gaps before it
	 * _q_tail points to the pkt with the highest seq num ever received
	 *
	 * In this example:
	 * 		segment_a->nxt == segment_b
	 * 		segment_b->nxt == segment_c
	 * 		segment_c->nxt == NULL
	 * 		segment_a->seq_next < segment_b->seq
	 * 		segment_b->seq_next == segment_c->seq
	 */

	/* CASE 1: Queue is empty */
	if (!_q_first){
		qe = new TCPQueueElt(p, pktlen, seq, seq_nxt);
		if (!qe) { return -2; }
		_q_first = _q_last = _q_tail = qe;
		qe->nxt = NULL;
		//debug_output(VERB_TCPQUEUE, "[%s] TCPQueue::push (empty)", _con->SPKRNAME);
		//debug_output(VERB_TCPQUEUE, "%s", pretty_print(sa, 60)->c_str());
		return 0;
	}

	// at this point we know the queue isn't empty

	/* CASE 2a: TAIL INSERT (we got a segment with seq number >= q_tail->seq_nxt) */
	if (SEQ_GEQ(seq, expected())){
		assert(!_q_tail->nxt);
		// bool perfect = false;

		/* CASE 2c: duplicate packet, discard */
		if (seq == expected() and seq_nxt == _q_tail->seq_nxt){
			assert (!_q_tail or !_q_tail->nxt);
			return -3;
		}

		/* enqueue after _q_tail */
		qe = new TCPQueueElt(p, pktlen, seq, seq_nxt);
		_q_tail->nxt = qe;

		/* CASE 2b: PERFECT TAIL INSERT (we got a segment with the next expected seq number) */
		if (seq == expected() && _q_last == _q_tail){
			_q_last = qe; /* if q_last is q_tail, then we drag q_last along */
			// perfect = true;
		}

		_q_tail = qe; /* qe becomes the new _q_tail */

		if (_q_last == NULL){
			loop_last();
		}

		//debug_output(VERB_TCPQUEUE, "[%s] TCPQueue::push (%s)", _con->SPKRNAME,
			// perfect?"perfect tail":"tail");
		//debug_output(VERB_TCPQUEUE, "%s", pretty_print(sa, 60)->c_str());
		return 0;
	}

	/* CASE 3: HEAD INSERT (we got a segment to be pushed at the front of the queue) */
	if (SEQ_LT(seq, first())){

	/* TCP Queue (Addresses (and seq num) decrease in this dir ->)
	 *
	 *					  -----------------
	 *					  |  new segment  |
	 *					  -----------------
	 *					  |overlap|  {seq}^
	 *		-------------------------
	 * .... |  segment  |  segment  | -> pull_front
	 *		-------------------------
	 *			{_q_last = _q_first}^
	 */

		/* If the packet overlaps with _q_first trim qe at end of packet */
		int overlap = (int)(seq_nxt - first());
		if (overlap > 0){
			if ((unsigned)overlap > pktlen) { return -2; }
			pktlen = pktlen - overlap;
			//debug_output(VERB_TCPQUEUE, "[%s] Tail overlap [%d] bytes", _con->SPKRNAME, overlap);
			seq_nxt -= overlap; // note seq_nxt has changed
			assert(seq_nxt == first());
		}

		qe = new TCPQueueElt(p, pktlen, seq, seq_nxt);
		qe->nxt = _q_first;
		_q_first = qe;

		// We are pushing in front of head which does not affect _q_last
		// UNLESS _q_last was set to NULL by pull_front. In this case, call loop
		// and _q_last will iteratively move from first toward q_tail until a gap
		// is found or we hit q_tail.
		if (_q_last == NULL){
			loop_last();
		}

		// If we have just made a gap by pushing at the head, set _q_last=_q_first
		if (_q_first->seq_nxt < _q_first->nxt->seq){
			_q_last = _q_first;
		}

		//debug_output(VERB_TCPQUEUE, "[%s] TCPQueue::push (head)", _con->SPKRNAME);
		//debug_output(VERB_TCPQUEUE, "%s", pretty_print(sa, 60)->c_str());
		return 0;
	}

	/* CASE 4: FILL A GAP (Default)
	 * KEEP IN MIND, this could also be a tail-enqueue where the packet head
	 * overlaps part of _q_tail */
	wrk = _q_first;
	// Try our luck - the gap might be right after _q_last
	if (_q_last && (seq == _q_last->seq_nxt)){
		wrk = _q_last;
	} else{
		// No luck, now we have to search from _q_first...
		// But first try to jump to q_last over any ordered part of the queue
		if (_q_last && SEQ_GT(seq, _q_last->seq_nxt)) { wrk = _q_last; }
		while (wrk->nxt && SEQ_GT(seq, wrk->nxt->seq)){
			// Move along the queue until we find where seq fits
			wrk = wrk->nxt;
		}
	}

	/* TCP Queue (Addresses (and seq num) decrease in this dir ->)
	 * Now wrk points to the segment before the gap where p should be enqueued
	 *
	 *			 -----------------
	 *			 |  new segment  |
	 *			 -----------------
	 *	  (gap between wrk and wrk->nxt)
	 *	  ------------------------------
	 * .... |   wrk->nxt   |	 wrk	 | ....
	 *		------------------------------
	 */

	// Test for overlap of front of packet with wrk
	int overlap = (int) (wrk->seq_nxt - seq);
	if (overlap > 0){
		if ((unsigned)overlap > pktlen) { return -2; }
		//debug_output(VERB_TCPQUEUE, "[%s] head overlap [%d] bytes", _con->SPKRNAME, overlap);
		memmove(p, p+overlap, pktlen-overlap);
		pktlen = pktlen-overlap;
		seq += overlap;
	}

	// If wrk->nxt exists test for overlap of back of packet with wrk->nxt
	if (wrk->nxt){
		overlap = (int) (seq_nxt - wrk->nxt->seq);
		if (overlap > 0){
			if ((unsigned)overlap > pktlen) { return -2; }
			//debug_output(VERB_TCPQUEUE, "[%s] Tail overlap [%d] bytes", _con->SPKRNAME, overlap);
			pktlen = pktlen-overlap;
			seq_nxt -= overlap;
		}
	}

	/* enqueue qe right after wrk */
	qe = new TCPQueueElt(p, pktlen, seq, seq_nxt);
	if (wrk->nxt){
		qe->nxt = wrk->nxt;
	}
	wrk->nxt = qe;

	if (wrk == _q_tail){ // make sure to keep the tail pointer up to date
		_q_tail = qe;
	}

	loop_last();

	//debug_output(VERB_TCPQUEUE, "[%s] TCPQueue::push (default)", _con->SPKRNAME);
	//debug_output(VERB_TCPQUEUE, "%s", pretty_print(sa, 60)->c_str());
	return 0;
}

/* In the case that we closed a gap, we can move _q_last toward _q_tail */
void
TCPQueue::loop_last()
{
	// If q_last is null, begin at q_first (important in CASE3)
	TCPQueueElt *wrk = (_q_last ? _q_last : _q_first);
	while (wrk->nxt && (wrk->seq_nxt == wrk->nxt->seq)){
		wrk = wrk->nxt;
		_q_last = wrk;
		//debug_output(VERB_TCPQUEUE, "Looping _q_last to [%u]", last());
	}
	_q_last = wrk;
	//debug_output(VERB_TCPQUEUE, "Looped _q_last to [%u]", last());
}

char *
TCPQueue::pull_front(uint32_t *len)
{
	char	*p = NULL;
	*len = 0;
	TCPQueueElt 	*e = NULL;

	// CASE 1: The queue is empty, nothing to pull
	if (_q_first == NULL){
		//debug_output(VERB_TCPQUEUE, "[%s] QPULL FIRST==NULL", _con->SPKRNAME);
		_q_tail = _q_last = NULL;
		return NULL;
	}

	// CASE 2: _q_last is NULL because we previously encountered CASE 3
	if (_q_last == NULL){
		//debug_output(VERB_TCPQUEUE, "[%s] QPULL LAST==NULL", _con->SPKRNAME);
		return NULL;
	}

	/* CASE 3: There is only one in-order packet to pull, return it and set
	 * _q_last = NULL to indicate that there is no more in-order data to pull
	 * after this pull */
	if (_q_first == _q_last){
		//debug_output(VERB_TCPQUEUE, "[%s] QPULL [%u] FIRST==LAST", _con->SPKRNAME, first());
		_q_last = NULL;
	} else{
		//debug_output(VERB_TCPQUEUE, "[%s] QPULL [%u]", _con->SPKRNAME, first());
	}
	if (_q_first == _q_tail){ // update tail so it doesn't dangle
		_q_tail = NULL;
	}

	e = _q_first;
	p = e->_p;
	*len = e->_plen;

	// _q_first becomes either the next QElt or NULL because of how push()
	// assigns _q_first->nxt
	_q_first = _q_first->nxt;

	delete e;

	return p;
}


StringAccum *
TCPQueue::pretty_print(StringAccum &sa, int signed_width)
{
	UNUSED(sa);
	tcp_seq_t head = 0;
	tcp_seq_t exp = 0;
	tcp_seq_t tail = 0;
	uint32_t i = 0;
	uint32_t width = (unsigned int) signed_width;
	TCPQueueElt * wp;
	// StringAccum stars;
	uint32_t thrd = width/3;

	if (width < 46){
		// sa << "Too narrow for prettyprinting";
		// return &sa;
	}
	if (_q_first){
		wp = _q_first;
		for (i = 0; i < width; i++){
			if (!wp){
				// stars << ".";
				continue;
			}
			if (wp == _q_first){
				head = i;
			}
			if (wp == _q_tail){
				exp = i;
			}
			if (wp == _q_last){
				tail = i;
			}
			if (wp->nxt && (wp->seq_nxt != wp->nxt->seq)){
				// stars << "*_";
				i++;
			} else{
				// stars << "*";
			}
			wp = wp->nxt;
		}
	} else{
		head = exp = tail = 0;
		// for(i = 0; i < width; i++)
			// stars << ".";
	}
 //	sa << "	 FIRST		LAST		TAIL\n";
	// sa.snprintf(36, "%10u  %10u  %10u\n", first(), last(), tailseq());
	// sa.snprintf(36, "%10u  %10u  %10u\n", _q_first->seq_nxt, last_nxt(), expected());

	for (i=0; i < width; i++){
		if (i == thrd || i == 2*thrd || i== 3*thrd){
			// sa << "|";
			continue;
		}
		if (((i < thrd && i >= head) || (i > thrd && i <= head)) ||
			((i < 2 * thrd && i >= exp) || (i > 2 * thrd && i <= exp)) ||
			((i < 3 * thrd  && i >= tail) || (i > 3 * thrd && i <= tail))){
			// sa << "_";
			continue;
		}
		// sa << " ";
	}
	// sa << "\n";
	// for (i = 0; i < width; i++ ) {
	// 	if (i == tail || i == exp || i == tail)
	// 		// sa << "|";
	// 	else
	// 		// sa << " ";
 //	}
	// sa << "\n" << stars;

	// return &sa;
	return NULL;
}


TCPFifo::TCPFifo(unsigned size)
{
	_size = size;
	_buf = (unsigned char *)calloc(size, sizeof(unsigned char));
	if (!_buf) {
		_size =  0;
	}

	_last = _buf + size;
	clear();
}


TCPFifo::~TCPFifo()
{
	//assert(_used == 0);
	if (_buf) {
		free(_buf);
		_buf = NULL;
	}
	clear();
}


int
TCPFifo::push(WritablePacket *p)
{
	unsigned count = p->length();
	unsigned char *data = p->data();

	//printf("pushing %u: h:%u t:%u f:%u u:%u\n", count, _h - _buf, _t - _buf, _free, _used);

	assert(_buf);

	// FIXME: we're in a weird packet vs byte thing here.
	// for now, don't accept if we can't take everything
	if (_free == 0 || count > _free) {
		return EWOULDBLOCK;
	}

	if (_h + count < _last) {
		// we can simply append
		memcpy(_h, data, count);
		_h += count;
		//printf("didn't wrap\n");
	} else {
		// the head is going to wrap
		unsigned num = _last - _h;

		// fill to end of buffer
		memcpy(_h, data, num);
		data += num;

		if (num > 0) {
			unsigned left = count - num;
			memcpy(_buf, data, left);
			_h = _buf + left;

			//printf("we had to wrap\n");
		} else {
			_h = _buf;
		}
	}

	_used += count;
	_free -= count;

	assert(_t < _last && _h < _last);
	assert(_free <= _size && _used <= _size);

	//printf("appended %u bytes, now %u bytes total head=%u\n", count, _used, _h -_buf);
	p->kill();
	return 0;
}


/* the function name lies: retval of 2 actually means "2 or more" */
int
TCPFifo::pkts_to_send(int offset, int win)
{
	assert(_buf);
	if (is_empty() || (offset >= win)) {
		return 0;
	} else {
		return 1;
	}

	// FIXME: in the old code, this would indicate that 2 or more
	// packets were in the fido. now that we are byte based this
	// doesn't make a lot of sense. Can we get away with it only
	// saying 0 or 1? Or does this need to be based on offset and
	// window or max payload size?
	// return 2;
}


/* get a piece of payload starting at <offset> bytes from the tail */
WritablePacket *
TCPFifo::get(tcp_seq_t offset, unsigned len)
{
	assert(_buf);
	assert(len <= _used);
	assert(offset <= _used);
	assert(offset + len <= _used);

	if (len > _used) {
		len = _used;
	}
	//printf("getting %u bytes from %u: h:%u t:%u f:%u u:%u\n", len, offset, _h - _buf, _t - _buf, _free, _used);

	WritablePacket *p = WritablePacket::make(512, NULL, len, 0);

	unsigned char *bytes = p->data();
	unsigned char *start = _t + offset;

	if (start > _last) {
		unsigned long s = start - _last;
		start = _buf + s;
		//printf("start is past end of buffer, wrapping to the front of the buffer: %lu\n", s);
	}

	if (start + len < _last) {
		//printf("copying all in one chunk\n");
		memcpy(bytes, start, len);

	} else {
		// we need to do it in 2 parts
		unsigned num = _last - start;
		memcpy(bytes, start, num);
		bytes += num;

		unsigned remaining = len - num;
		//printf("buffer wraps: %d from end, %d from front\n", num, remaining);
		memcpy(bytes, _buf, remaining);
	}

	return p;
}


/* Drop <offset> bytes from tail of the fifo by killing the packet and possibly
 * taking excess bytes from the last packet */
void
TCPFifo::drop_until(tcp_seq_t offset)
{
	assert(_buf);
	assert(offset <= _used);

	//printf("dropping %u: h:%u t:%u f:%u u:%u\n", offset, _h - _buf, _t - _buf, _free, _used);
	if (offset >= _used) {
		//printf("dropped everything!\n");
		// the queue is now empty
		_used = 0;
		_free = _size;
		_h = _t = _buf;

	} else {
		// advance the tail pointer
		_t += offset;

		//printf("advancing tail by %u\n", offset);

		// check to see if the tail should wrap
		if (_t >= _last) {
			//printf("t > last %u %p %p\n", _t - _last, _t, _last);
			unsigned s = _t - _last;
			_t = _buf + s;
			//printf("buf = %p t=%p diff=%u\n", _buf, _t, _t - _buf);
		}

		_free += offset;
		_used -= offset;
	}
	//printf("released %u: h:%u t:%u f:%u u:%u\n", offset, _h - _buf, _t - _buf, _free, _used);
}


CLICK_ENDDECLS

EXPORT_ELEMENT(XStream)
ELEMENT_REQUIRES(userlevel)
