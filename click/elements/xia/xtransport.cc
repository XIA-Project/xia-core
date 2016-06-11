#include "../../userlevel/xia.pb.h"
#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/error-syslog.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>
//#include <click/xiacontentheader.hh>
#include "xtransport.hh"
#include <click/xiastreamheader.hh>
#include <click/xiatransportheader.hh>
#include "xlog.hh"
#include "xdatagram.hh"
#include "xstream.hh"
//#include "xchunk.hh"
#include <click/xiasecurity.hh>  // xs_getSHA1Hash()

/*
** FIXME:
** - set saner retransmit values before we get backoff code
** - implement a backoff delay on retransmits so we don't flood the connection
** - fix cid header size issue so we work correctly with the linux version
** - if we receive a duplicate SYN, should it reset the SYNACK retransmit count to 0?
** - check for memory leaks (slow leak caused by open/close of stream sockets)
** - see various FIXMEs in the code
** - get sk in ProcessAPIPacket instead of each individual handler
** - replace copy_packet with uniqueify. not needed for migration in reliable transport
**	 still needed for datagram and cid??
*/

CLICK_DECLS

sock::sock(
	XTRANSPORT *trans,
	unsigned short apiport,
	int type) : hstate(CREATE) {
	state = INACTIVE;
	reap = false;
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
	if (type == SOCK_STREAM)
		nxt_xport = CLICK_XIA_NXT_XTCP;
	else
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
	port = apiport;
	transport = trans;
	sock_type = type;
	_errh = transport -> error_handler();
	hlim = HLIM_DEFAULT;
	if (type == SOCK_STREAM)
		nxt = CLICK_XIA_NXT_XTCP;
	else
		nxt = CLICK_XIA_NXT_TRN;
	refcount = 1;
	xcacheSock = false;
}

sock::sock() {
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
	nxt_xport = CLICK_XIA_NXT_NO;
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
	refcount = 1;
	xcacheSock = false;
}


XTRANSPORT::XTRANSPORT() : _timer(this)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	cp_xid_type("SID", &_sid_type);	// FIXME: why isn't this a constant?
}



int XTRANSPORT::configure(Vector<String> &conf, ErrorHandler *errh)
{
	XIAPath local_addr;
	XID local_4id;
	Element* routing_table_elem;
	bool is_dual_stack_router;
	_is_dual_stack_router = false;
    char xidString[50];

	/* Configure tcp relevant information */
	memset(&_tcpstat, 0, sizeof(_tcpstat));
	_errhandler = errh;

	/* _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router()); */

	_tcp_globals.tcp_keepidle 		= 120;
	_tcp_globals.tcp_keepintvl 		= 120;
	_tcp_globals.tcp_maxidle   		= 120;
	_tcp_globals.tcp_now 			= 0;
	_tcp_globals.so_recv_buffer_size = 0x10000;
	_tcp_globals.tcp_mssdflt		= 1024;
	_tcp_globals.tcp_rttdflt		= TCPTV_SRTTDFLT / PR_SLOWHZ;
	_tcp_globals.so_flags	   	 	= 0;
	_tcp_globals.so_idletime		= 0;
	_verbosity 						= VERB_ERRORS;

	bool so_flags_array[32];
	bool t_flags_array[10];
	memset(so_flags_array, 0, 32 * sizeof(bool));
	memset(t_flags_array, 0, 10 * sizeof(bool));

	assert (cp_va_kparse(conf, this, errh,
						 "LOCAL_ADDR", cpkP + cpkM, cpXIAPath, &local_addr,
						 "LOCAL_4ID", cpkP + cpkM, cpXID, &local_4id,
						 "ROUTETABLENAME", cpkP + cpkM, cpElement, &routing_table_elem,
						 "IS_DUAL_STACK_ROUTER", 0, cpBool, &is_dual_stack_router,
						 "IDLETIME", 0, cpUnsigned, &(_tcp_globals.so_idletime),
						 "MAXSEG", 	0, cpUnsignedShort, &(_tcp_globals.tcp_mssdflt),
						 "RCVBUF", 	0, cpUnsigned, &(_tcp_globals.so_recv_buffer_size),
						 "WINDOW_SCALING", 0, cpUnsigned, &(_tcp_globals.window_scale),
						 "USE_TIMESTAMPS", 0, cpBool, &(_tcp_globals.use_timestamp),
						 "FIN_AFTER_TCP_FIN",  0, cpBool, &(so_flags_array[8]),
						 "FIN_AFTER_TCP_IDLE", 0, cpBool, &(so_flags_array[9]),
						 "FIN_AFTER_UDP_IDLE", 0, cpBool, &(so_flags_array[10]),
						 "VERBOSITY", 0, cpUnsigned, &(_verbosity), // not sure we need this
						 cpEnd) >= 0);
	// return -1;

	for (int i = 0; i < 32; i++) {
		if (so_flags_array[i])
			_tcp_globals.so_flags |= ( 1 << i ) ;
	}
	_tcp_globals.so_idletime *= PR_SLOWHZ;
	if (_tcp_globals.window_scale > TCP_MAX_WINSHIFT)
		_tcp_globals.window_scale = TCP_MAX_WINSHIFT;

	_local_addr = local_addr;
	_local_hid = local_addr.xid(local_addr.destination_node());

    /* TODO: How should we choose xcacheSid? */
    random_xid("SID", xidString);
    _xcache_sid.parse(xidString);

	_local_4id = local_4id;

	// IP:0.0.0.0 indicates NULL 4ID
	_null_4id.parse("IP:0.0.0.0");

	_is_dual_stack_router = is_dual_stack_router;

#if USERLEVEL
	_routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
#else
	_routeTable = reinterpret_cast<XIAXIDRouteTable*>(routing_table_elem);
#endif

	return 0;
}



XTRANSPORT::~XTRANSPORT()
{
	// click is shutting down, so we can get away with being lazy here
	//Clear all hashtable entries
	XIDtoSock.clear();
	portToSock.clear();
	XIDtoPushPort.clear();
	XIDpairToSock.clear();
	XIDpairToConnectPending.clear();

	xcmp_listeners.clear();
	notify_listeners.clear();
}



int XTRANSPORT::initialize(ErrorHandler *errh)
{
	// XLog installed the syslog error handler, use it!
	_errh = (SyslogErrorHandler*)ErrorHandler::default_handler();
	_timer.initialize(this);

	_fast_ticks = new Timer(this);
	_fast_ticks->initialize(this);
	_fast_ticks->schedule_after_msec(TCP_FAST_TICK_MS);

	_slow_ticks = new Timer(this);
	_slow_ticks->initialize(this);
	_slow_ticks->schedule_after_msec(TCP_SLOW_TICK_MS);

	_reaper = new Timer(this);
	_reaper->initialize(this);
	_reaper->schedule_after_msec(700);

	_errhandler = errh;
	return 0;
}



void XTRANSPORT::push(int port, Packet *p_input)
{
	WritablePacket *p_in = p_input->uniqueify();

	switch(port) {
		case API_PORT:	// control packet from socket API
			ProcessAPIPacket(p_in);
			break;

		case BAD_PORT: //packet from ???
			ERROR("\n\nERROR: BAD INPUT PORT TO XTRANSPORT!!!\n\n");
			break;

		case NETWORK_PORT: //Packet from network layer
			ProcessNetworkPacket(p_in);
			p_in->kill();
			break;

//		case CACHE_PORT:	//Packet from cache
//			ProcessCachePacket(p_in);
//			p_in->kill();
//			break;

		case XHCP_PORT:		//Packet with DHCP information
			ProcessXhcpPacket(p_in);
			p_in->kill();
			break;

		default:
			ERROR("packet from unknown port: %d\n", port);
			break;
	}
}



/*************************************************************
** HANDLER FUNCTIONS
*************************************************************/
// ?????????
enum {H_MOVE};

int XTRANSPORT::write_param(const String &conf, Element *e, void *vparam, ErrorHandler *errh)
{
	XTRANSPORT *f = static_cast<XTRANSPORT *>(e);

	switch (reinterpret_cast<intptr_t>(vparam)) {
	case H_MOVE:
	{
		XIAPath local_addr;
		if (cp_va_kparse(conf, f, errh,
						 "LOCAL_ADDR", cpkP + cpkM, cpXIAPath, &local_addr,
						 cpEnd) < 0)
			return -1;
		f->_local_addr = local_addr;
		errh->message("Moved to %s", local_addr.unparse().c_str());
		f->_local_hid = local_addr.xid(local_addr.destination_node());

	}
	break;
	default:
		break;
	}
	return 0;
}



int XTRANSPORT::purge(const String & /*conf */, Element *e, void *thunk, ErrorHandler * /*errh */)
{
	XTRANSPORT* xt = static_cast<XTRANSPORT*>(e);
	int count = 0;
	int purge = thunk != 0;

	// If purge is true, kill all stream sockets
	// else kill those in TIME_WAIT state

	for (HashTable<unsigned short, sock*>::iterator it = xt->portToSock.begin(); it != xt->portToSock.end(); ++it) {
		//unsigned short _sport = it->first;
		sock *sk = it->second;

		if (sk->sock_type == SOCK_STREAM) {
			if (purge || sk->state == TIME_WAIT) {
				count++;
				xt->_errh->warning("purging %d\n", sk->port);
				xt->TeardownSocket(sk);
			}
		}
	}
	return count;
}



String XTRANSPORT::Netstat(Element *e, void *)
{
	String table;
	char line[512];
	XTRANSPORT* xt = static_cast<XTRANSPORT*>(e);

	for (HashTable<unsigned short, sock*>::iterator it = xt->portToSock.begin(); it != xt->portToSock.end(); ++it) {
		unsigned short _sport = it->first;
		sock *sk = it->second;
		const char *type = SocketTypeStr(sk->sock_type);
		const char *state = "";
		const char *xid = "";
		XID source_xid;

		if (sk->sock_type == SOCK_STREAM) {
			state = StateStr(sk->state);
		}

		if ((sk->src_path.destination_node()) != static_cast<size_t>(-1)) {
			source_xid = sk->src_path.xid(sk->src_path.destination_node());
			xid = source_xid.unparse().c_str();
		}

		sprintf(line, "%d,%s,%s,%s,%d\n", _sport, type, state, xid, sk->refcount);
		table += line;
	}

	return table;
}



void XTRANSPORT::add_handlers()
{
	add_write_handler("local_addr", write_param, (void *)H_MOVE);
	add_write_handler("purge", purge, (void*)1);
	add_write_handler("flush", purge, 0);
	add_read_handler("netstat", Netstat, 0);
}



/*************************************************************
** HELPER FUNCTIONS
*************************************************************/
Packet *XTRANSPORT::UDPIPPrep(Packet *p_in, int dport)
{
	p_in->set_dst_ip_anno(IPAddress("127.0.0.1"));
	SET_DST_PORT_ANNO(p_in, dport);

	return p_in;
}



char *XTRANSPORT::random_xid(const char *type, char *buf)
{
	// This is a stand-in function until we get certificate based names
	//
	// note: buf must be at least 45 characters long
	// (longer if the XID type gets longer than 3 characters)
	sprintf(buf, RANDOM_XID_FMT, type, click_random(0, 0xffffffff));

	return buf;
}



const char *XTRANSPORT::SocketTypeStr(int stype)
{
	const char *s = "???";
	switch (stype) {
		case SOCK_STREAM: s = "STREAM"; break;
		case SOCK_DGRAM:  s = "DGRAM";  break;
		case SOCK_RAW:	s = "RAW";	break;
		case SOCK_CHUNK:  s = "CHUNK";  break;
	}
	return s;
}



const char *XTRANSPORT::StateStr(SocketState state)
{
	const char *s = "???";
	switch(state) {
		case INACTIVE:   s = "INACTIVE";   break;
		case LISTEN:	 s = "LISTEN";	 break;
		case SYN_RCVD:   s = "SYN_RCVD";   break;
		case SYN_SENT:   s = "SYN_SENT";   break;
		case CONNECTED:  s = "CONNECTED";  break;
		case FIN_WAIT1:  s = "FIN_WAIT1";  break;
		case FIN_WAIT2:  s = "FIN_WAIT2";  break;
		case TIME_WAIT:  s = "TIME_WAIT";  break;
		case CLOSING:	s = "CLOSING";	break;
		case CLOSE_WAIT: s = "CLOSE_WAIT"; break;
		case LAST_ACK:   s = "LAST_ACK";   break;
		case CLOSED:	 s = "CLOSED";	 break;
	}
	return s;
}



void XTRANSPORT::ChangeState(sock *sk, SocketState state)
{
	INFO("socket %d changing state from %s to %s\n", sk->port, StateStr(sk->state), StateStr(state));
	sk->state = state;
}



void XTRANSPORT::copy_common(sock *sk, XIAHeader &xiahdr, XIAHeaderEncap &xiah)
{
	//Recalculate source path
	XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
	String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse();
	//Make source DAG _local_addr:SID
	String dagstr = sk->src_path.unparse_re();

	//Client Mobility...
	if (dagstr.length() != 0 && dagstr != str_local_addr) {
		//Moved!
		// 1. Update 'sk->src_path'
		sk->src_path.parse_re(str_local_addr);
	}

	xiah.set_nxt(xiahdr.nxt());
	xiah.set_last(xiahdr.last());
	xiah.set_hlim(xiahdr.hlim());
	xiah.set_dst_path(sk->dst_path);
	xiah.set_src_path(sk->src_path);
	xiah.set_plen(xiahdr.plen());
}



WritablePacket *XTRANSPORT::copy_packet(Packet *p, sock *sk)
{
	UNUSED(p);
	UNUSED(sk);
#if 0
	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;

	copy_common(sk, xiahdr, xiah);

	TransportHeader thdr(p);
	TransportHeaderEncap *new_thdr = new TransportHeaderEncap(thdr.type(), thdr.pkt_info(), thdr.seq_num(), thdr.ack_num(), thdr.length(), thdr.recv_window());

	WritablePacket *copy = WritablePacket::make(256, thdr.payload(), xiahdr.plen() - thdr.hlen(), 20);

	copy = new_thdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete new_thdr;

	return copy;
#endif
	return NULL;
}



WritablePacket *XTRANSPORT::copy_cid_req_packet(Packet *p, sock *sk)
{
	UNUSED(p);
	UNUSED(sk);
#if 0
	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;
	copy_common(sk, xiahdr, xiah);

	WritablePacket *copy = WritablePacket::make(256, xiahdr.payload(), xiahdr.plen(), 20);

	ContentHeaderEncap *chdr = ContentHeaderEncap::MakeRequestHeader();

	copy = chdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete chdr;
	xiah.set_plen(xiahdr.plen());

	return copy;
#endif
	return NULL;
}



WritablePacket *XTRANSPORT::copy_cid_response_packet(Packet *p, sock *sk)
{
	UNUSED(p);
	UNUSED(sk);
#if 0
	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;
	copy_common(sk, xiahdr, xiah);

	WritablePacket *copy = WritablePacket::make(256, xiahdr.payload(), xiahdr.plen(), 20);

	ContentHeader chdr(p);
	ContentHeaderEncap *new_chdr = new ContentHeaderEncap(chdr.opcode(), chdr.chunk_offset(), chdr.length());

	copy = new_chdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete new_chdr;
	xiah.set_plen(xiahdr.plen());

	return copy;
#endif
	return NULL;
}



bool XTRANSPORT::TeardownSocket(sock *sk)
{
	INFO("TeardownSocket is called");
	XID src_xid;
	XID dst_xid;
	bool have_src = 0;
	bool have_dst = 0;

	INFO("Tearing down %s socket %d\n", SocketTypeStr(sk->sock_type), sk->port);

	CancelRetransmit(sk);

	if (sk->src_path.destination_node() != static_cast<size_t>(-1)) {
		src_xid = sk->src_path.xid(sk->src_path.destination_node());
		have_src = true;
	}
	if (sk->dst_path.destination_node() != static_cast<size_t>(-1)) {
		dst_xid = sk->dst_path.xid(sk->dst_path.destination_node());
		have_dst = true;
	}

	xcmp_listeners.remove(sk->port);

	if (sk->sock_type == SOCK_STREAM) {
		if (have_src && have_dst) {
			XIDpair xid_pair;
			xid_pair.set_src(src_xid);
			xid_pair.set_dst(dst_xid);

			XIDpairToConnectPending.erase(xid_pair);
			XIDpairToSock.erase(xid_pair);
		}

		// FIXME:delete these too
		//queue<sock*> pending_connection_buf;
		//queue<xia::XSocketMsg*> pendingAccepts;
		// MERGE - why is this commented out now?
		// for (int i = 0; i < sk->send_buffer_size; i++) {
		// 	if (sk->send_buffer[i] != NULL) {
		// 		sk->send_buffer[i]->kill();
		// 		sk->send_buffer[i] = NULL;
		// 	}
		// }
	}

	if (!sk->isAcceptedSocket) {
		// we only do this if the socket wasn't generateed due to an accept

		if (have_src) {
			DBG("deleting route for %d %s\n", sk->port, src_xid.unparse().c_str());
			delRoute(src_xid);
			XIDtoSock.erase(src_xid);
		}
	}

	if (sk->sock_type == SOCK_CHUNK) {
		// FIXME: delete this stuff of chunk sockets will leak!
		//HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
		//HashTable<XID, Timestamp> XIDtoExpiryTime;
		//HashTable<XID, bool> XIDtoTimerOn;
		//HashTable<XID, int> XIDtoStatus;	// Content-chunk request status... 1: waiting to be read, 0: waiting for chunk response, -1: failed
		//HashTable<XID, bool> XIDtoReadReq;	// Indicates whether ReadCID() is called for a specific CID
		//HashTable<XID, WritablePacket*> XIDtoCIDresponsePkt;
	}

	portToSock.erase(sk->port);

	// MERGE - why is this commented out now?
	// for (int i = 0; i < sk->recv_buffer_size; i++) {
	// 	if (sk->recv_buffer[i] != NULL) {
	// 		sk->recv_buffer[i]->kill();
	// 		sk->recv_buffer[i] = NULL;
	// 	}
	// }

	delete sk;
	return true;
}


/*************************************************************
** RETRANSMIT CODE
*************************************************************/
void XTRANSPORT::ScheduleTimer(sock *sk, int delay)
{
	sk->timer_on = true;
	sk->expiry = Timestamp::now() + Timestamp::make_msec(delay);

	if (! _timer.scheduled() || _timer.expiry() >= sk->expiry)
		_timer.reschedule_at(sk->expiry);
}



void XTRANSPORT::CancelRetransmit(sock *sk)
{
	sk->num_connect_tries = 0;
	sk->num_retransmits = 0;
	sk->num_close_tries = 0;
	sk->timer_on = false;

	if (sk->pkt) {
		sk->pkt->kill();
		sk->pkt = NULL;
	}
}




bool XTRANSPORT::RetransmitMIGRATE(sock *sk, unsigned short _sport, Timestamp &now)
{
	UNUSED(sk);
	UNUSED(_sport);
	UNUSED(now);
	bool rc = false;
#if 0
	if (sk->num_migrate_tries <= MAX_RETRANSMIT_TRIES) {
		DBG("Socket %d MIGRATE retransmit\n", _sport);

		sk->timer_on = true;
		sk->migrateack_waiting = true;
		sk->expiry = now + Timestamp::make_msec(MIGRATEACK_DELAY);
		sk->num_migrate_tries++;

		WritablePacket *copy = copy_packet(sk->migrate_pkt, sk);
		output(NETWORK_PORT).push(copy);

	} else {
		WARN("Socket %d MIGRATE retransmit count exceeded\n", _sport);
		// FIXME: send RST?
		CancelRetransmit(sk);
		sk->migrateack_waiting = false;
		rc = true;
	}
#endif
	return rc;
}

void XTRANSPORT::RetransmitCIDRequest(sock *sk, Timestamp &now, Timestamp &earliest_pending_expiry)
{
	for (HashTable<XID, bool>::iterator it = sk->XIDtoTimerOn.begin(); it != sk->XIDtoTimerOn.end(); ++it ) {
		XID requested_cid = it->first;
		bool timer_on = it->second;

		HashTable<XID, Timestamp>::iterator it2;
		it2 = sk->XIDtoExpiryTime.find(requested_cid);
		Timestamp cid_req_expiry = it2->second;

		if (timer_on == true && cid_req_expiry <= now) {
			DBG("Socket %d  Chunk RETRANSMIT (%s)", sk->port);

			//retransmit cid-request
			HashTable<XID, WritablePacket*>::iterator it3;
			it3 = sk->XIDtoCIDreqPkt.find(requested_cid);
			WritablePacket *copy = copy_cid_req_packet(it3->second, sk);
			output(NETWORK_PORT).push(copy);

			cid_req_expiry  = Timestamp::now() + Timestamp::make_msec(ACK_DELAY);
			sk->XIDtoExpiryTime.set(requested_cid, cid_req_expiry);
			sk->XIDtoTimerOn.set(requested_cid, true);
		}

		if (timer_on == true && cid_req_expiry > now && ( cid_req_expiry < earliest_pending_expiry || earliest_pending_expiry == now ) ) {
			earliest_pending_expiry = cid_req_expiry;
		}
	}
}

void XTRANSPORT::run_timer(Timer *timer)
{
	ConnIterator i = XIDpairToSock.begin();
	ConnIterator j = XIDpairToConnectPending.begin();
	XStream *con = NULL;

	if (timer == _fast_ticks) {
		for (; i; i++) {
			if (i->second->get_type() == SOCK_STREAM &&
					!i->second->reap)
			{
				con = dynamic_cast<XStream *>(i->second);
				con->fasttimo();
			}
		}
		for (; j; j++) {
			if (j->second->get_type() == SOCK_STREAM &&
					!j->second->reap)
			{
				con = dynamic_cast<XStream *>(j->second);
				con->fasttimo();
			}
		}
		_fast_ticks->reschedule_after_msec(TCP_FAST_TICK_MS);
	} else if (timer == _slow_ticks) {
		for (; i; i++) {
			if (i->second->get_type() == SOCK_STREAM &&
					!i->second->reap)
			{
				con = dynamic_cast<XStream *>(i->second);
				con->slowtimo();
			}
		}
		for (; j; j++) {
			if (j->second->get_type() == SOCK_STREAM &&
					!j->second->reap)
			{
				con = dynamic_cast<XStream *>(j->second);
				con->slowtimo();
			}
		}
		_slow_ticks->reschedule_after_msec(TCP_SLOW_TICK_MS);
		(globals()->tcp_now)++;
	} else if (timer == _reaper) {
		for (; i; i++) {
			// INFO("This is %d, %d",i->second->port,i->second->reap);
			if (i->second->reap)
			{
				INFO("Going to remove %d", i->second->port);
				TeardownSocket(i->second);
			}
		}
		_reaper->schedule_after_msec(700);
		// debug_output(VERB_TIMERS, "%u: XTRANSPORT::run_timer: unknown timer", tcp_now());
	}
	return;

}



/*************************************************************
** BUFFER MANAGEMENT
*************************************************************/
/**
* @brief Calculates a connection's loacal receive window.
*
* recv_window = recv_buffer_size - (next_seqnum - base)
*
* @param sk
*
* @return The receive window.
*/
uint32_t XTRANSPORT::calc_recv_window(sock *sk)
{
	return sk->recv_buffer_size - (sk->next_recv_seqnum - sk->recv_base);
}

/**
* @brief Checks whether or not a received packet can be buffered.
*
* Checks if we have room to buffer the received packet; that is, is the packet's
* sequence number within our recieve window? (Or, in the case of a DGRAM socket,
* simply checks if there is an unused slot at the end of the recv buffer.)
*
* @param p
* @param sk
*
* @return true if packet can be buffered, false otherwise
*/
bool XTRANSPORT::should_buffer_received_packet(WritablePacket *p, sock *sk)
{
	if (sk->sock_type == SOCK_STREAM) {
		// check if received_seqnum is within our current recv window
		// TODO: if we switch to a byte-based, buf size, this needs to change
		StreamHeader thdr(p);
		unsigned received_seqnum = thdr.seq_num();
		if (received_seqnum >= sk->next_recv_seqnum &&
			received_seqnum < sk->next_recv_seqnum + sk->recv_buffer_size) {
			return true;
		}
	} else if (sk->sock_type == SOCK_DGRAM) {

		if (sk->recv_buffer_count < sk->recv_buffer_size) {
			return true;
		}
	} else if (sk->sock_type == SOCK_RAW) {
		if (sk->recv_buffer_count < sk->recv_buffer_size) {
			return true;
		}
	}
	return false;
}

/**
* @brief Adds a packet to the connection's receive buffer.
*
* Stores the supplied packet pointer, p, in a slot depending on sock type:
*
*   STREAM: index = seqnum % bufsize.
*   DGRAM:  index = (end + 1) % bufsize
*
* @param p
* @param sk
*/
void XTRANSPORT::add_packet_to_recv_buf(WritablePacket *p, sock *sk)
{
	int index = -1;
	if (sk->sock_type == SOCK_STREAM) {
		StreamHeader thdr(p);
		int received_seqnum = thdr.seq_num();
		index = received_seqnum % sk->recv_buffer_size;

	} else if (sk->sock_type == SOCK_DGRAM || sk->sock_type == SOCK_RAW) {
		index = (sk->dgram_buffer_end + 1) % sk->recv_buffer_size;
		sk->dgram_buffer_end = index;
		sk->recv_buffer_count++;
	}

	WritablePacket *p_cpy = p->clone()->uniqueify();
	sk->recv_buffer[index] = p_cpy;
}

/**
* @brief check to see if the app is waiting for this data; if so, return it now
*
* @param sk
*/
void XTRANSPORT::check_for_and_handle_pending_recv(sock *sk)
{
	if (sk->recv_pending) {
		int bytes_returned = read_from_recv_buf(sk->pending_recv_msg, sk);
		ReturnResult(sk->port, sk->pending_recv_msg, bytes_returned);

		sk->recv_pending = false;
		delete sk->pending_recv_msg;
		sk->pending_recv_msg = NULL;
	}
}


void XTRANSPORT::resize_buffer(WritablePacket* buf[], int max, int type, uint32_t old_size, uint32_t new_size, int *dgram_start, int *dgram_end)
{
	if (new_size < old_size) {
		WARN("new buffer size is smaller than old size. Some data may be discarded.\n");
		old_size = new_size; // so we stop after moving as many packets as will fit in the new buffer
	}

	// General procedure: make a temporary buffer and copy pointers to their
	// new indices in the temp buffer. Then, rewrite the original buffer.
	WritablePacket *temp[max];
	memset(temp, 0, max);

	// Figure out the new index for each packet in buffer
	int new_index = -1;
	for (unsigned i = 0; i < old_size; i++) {
		if (type == SOCK_STREAM) {
			StreamHeader thdr(buf[i]);
			new_index = thdr.seq_num() % new_size;
		} else if (type == SOCK_DGRAM) {
			new_index = (i + *dgram_start) % old_size;
		}
		temp[new_index] = buf[i];
	}

	// For DGRAM socket, reset start and end vars
	if (type == SOCK_DGRAM) {
		*dgram_start = 0;
		*dgram_end = (*dgram_start + *dgram_end) % old_size;
	}

	// Copy new locations from temp back to original buf
	memset(buf, 0, max);
	for (int i = 0; i < max; i++) {
		buf[i] = temp[i];
	}
}



void XTRANSPORT::resize_send_buffer(sock *sk, uint32_t new_size)
{
	resize_buffer(sk->send_buffer, MAX_SEND_WIN_SIZE, sk->sock_type, sk->send_buffer_size, new_size, &(sk->dgram_buffer_start), &(sk->dgram_buffer_end));
	sk->send_buffer_size = new_size;
}



void XTRANSPORT::resize_recv_buffer(sock *sk, uint32_t new_size)
{
	resize_buffer(sk->recv_buffer, MAX_RECV_WIN_SIZE, sk->sock_type, sk->recv_buffer_size, new_size, &(sk->dgram_buffer_start), &(sk->dgram_buffer_end));
	sk->recv_buffer_size = new_size;
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
* @param sk The sock struct for this connection
*
* @return  The number of bytes read from the buffer.
*/
int XTRANSPORT::read_from_recv_buf(xia::XSocketMsg *xia_socket_msg, sock *sk)
{
	if (sk->sock_type == SOCK_STREAM) {

		xia::X_Recv_Msg *x_recv_msg = xia_socket_msg->mutable_x_recv();
		int bytes_requested = x_recv_msg->bytes_requested();
		bool peek = x_recv_msg->flags() & MSG_PEEK;
		int bytes_returned = 0;

		// FIXME - this should use the recv buffer size
		char buf[64 * 1024]; // TODO: pick a buf size
		memset(buf, 0, 64 * 1024);
		unsigned i;

		// FIXME: make sure bytes requested is <= recv buffer size

		for (i = sk->recv_base; i < sk->next_recv_seqnum; i++) {

			if (bytes_returned >= bytes_requested) break;

			WritablePacket *p = sk->recv_buffer[i % sk->recv_buffer_size];
			XIAHeader xiah(p->xia_header());
			StreamHeader thdr(p);
			size_t data_size = xiah.plen() - thdr.hlen();

			const char *payload = (char *)thdr.payload();
			uint16_t tail = XIA_TAIL_ANNO(p);

			if (tail) {
				DBG("%d: packet (%d) has %d bytes of %d remaining\n", sk->port, i % sk->recv_buffer_size, data_size - tail, data_size);
				data_size -= tail;
				payload += tail;
			}

			memcpy((void*)(&buf[bytes_returned]), (const void *)payload, data_size);
			bytes_returned += data_size;

			// leave the data if the user peeked
			if (!peek) {
				if (bytes_returned <= bytes_requested) {
					// it's safe to delete this packet
					p->kill();
					sk->recv_buffer[i % sk->recv_buffer_size] = NULL;
					sk->recv_base++;

				} else {
					// we need to keep the tail data the application didn't ask for
					// update this packet to shrink the data
					int extra = bytes_returned - bytes_requested;
					tail = xiah.plen() - thdr.hlen() - extra;

					DBG("%d: keeping the last %d bytes in packet %d\n", sk->port, extra, i % sk->recv_buffer_size);
					SET_XIA_TAIL_ANNO(p, tail);
				}
			} else {
				DBG("peeking, so leaving all data behind for packet %d\n", i % sk->recv_buffer_size);
			}
		}

		x_recv_msg->set_payload(buf, bytes_returned);
		x_recv_msg->set_bytes_returned(bytes_returned);

		DBG("%d: returning %d bytes out of %d requested\n", sk->port, bytes_returned, bytes_requested);

		return bytes_returned;

	} else if (sk->sock_type == SOCK_DGRAM || sk->sock_type == SOCK_RAW) {
		xia::X_Recvfrom_Msg *x_recvfrom_msg = xia_socket_msg->mutable_x_recvfrom();

		bool peek = x_recvfrom_msg->flags() & MSG_PEEK;

		// Get just the next packet in the recv buffer (we don't return data from more
		// than one packet in case the packets came from different senders). If no
		// packet is available, we indicate to the app that we returned 0 bytes.
		WritablePacket *p = sk->recv_buffer[sk->dgram_buffer_start];

		if (sk->recv_buffer_count > 0 && p) {
			// get different sized packages depending on socket type
			// datagram only wants payload
			// raw wants transport header too
			// packet wants it all
			XIAHeader xiah(p->xia_header());
			TransportHeader thdr(p);
			int data_size;
			String payload;

			switch (sk->sock_type) {
				case SOCK_DGRAM:
					data_size = xiah.plen() - thdr.hlen();
					payload = String((const char*)thdr.payload(), data_size);
					break;

				case SOCK_RAW:
				{
					String header((const char*)xiah.hdr(), xiah.hdr_size());
					String data((const char*)xiah.payload(), xiah.plen());
					payload = header + data;
					data_size = payload.length();

				}
					break;

				default:
					// this should not be possible
					data_size = 0;
					break;
			}

			// this part is the same for everyone
			String src_path = xiah.src_path().unparse();

			uint16_t iface = SRC_PORT_ANNO(p);

			x_recvfrom_msg->set_interface_id(iface);
			x_recvfrom_msg->set_payload(payload.c_str(), payload.length());
			x_recvfrom_msg->set_sender_dag(src_path.c_str());
			x_recvfrom_msg->set_bytes_returned(data_size);

			if (!peek) {
				// NOTE: bytes beyond what the app asked for will be discarded,
				// they are not saved for the next recv like streaming socket data

				p->kill();
				sk->recv_buffer[sk->dgram_buffer_start] = NULL;
				sk->recv_buffer_count--;
				sk->dgram_buffer_start = (sk->dgram_buffer_start + 1) % sk->recv_buffer_size;
			}

			return data_size;

		} else {
			x_recvfrom_msg->set_bytes_returned(0);
			return 0;
		}
	}

	return -1;
}



/*************************************************************
** XHCP PACKET HANDLER
*************************************************************/
void XTRANSPORT::ProcessXhcpPacket(WritablePacket *p_in)
{
	WARN("IS THIS USED ANYMORE?\n");
	XIAHeader xiah(p_in->xia_header());
	String temp = _local_addr.unparse();
	Vector<String> ids;
	cp_spacevec(temp, ids);;
	if (ids.size() < 3) {
		String new_route((char *)xiah.payload());
		String new_local_addr = new_route + " " + ids[1];
		_local_addr.parse(new_local_addr);
	}
}



/*************************************************************
** NETWORK PACKET HANDLER
*************************************************************/
void XTRANSPORT::ProcessNetworkPacket(WritablePacket *p_in)
{
	XIAHeader xiah(p_in->xia_header());

	switch(xiah.nxt()) {
		case CLICK_XIA_NXT_XCMP:
			// pass the packet to all sockets that registered for XMCP packets
			ProcessXcmpPacket(p_in);
			return;

		case CLICK_XIA_NXT_XTCP:
			ProcessStreamPacket(p_in);
			return;

		default:
			break;
	}

	TransportHeader thdr(p_in);

	switch(thdr.type()) {
		case SOCK_DGRAM:
			ProcessDatagramPacket(p_in);
			break;

		default:
			WARN("ProcessNetworkPacket: Unknown TransportType:%d\n", thdr.type());
	}
}


/*************************************************************
** DATAGRAM PACKET HANDLER
*************************************************************/
void XTRANSPORT::ProcessDatagramPacket(WritablePacket *p_in)
{
	XIAHeader xiah(p_in->xia_header());
	XIAPath dst_path = xiah.dst_path();
	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);

	sock *sk = XID2Sock(_destination_xid);  // This is to be updated for the XSOCK_STREAM type connections below

	if (!sk) {
		WARN("ProcessDatagramPacket: sk == NULL\n");
		return;
	}
	dynamic_cast<XDatagram *>(sk)->push(p_in);
}


/*************************************************************
** STREAMING TRANSPORT PACKET HANDLERS
*************************************************************/
void XTRANSPORT::ProcessStreamPacket(WritablePacket *p_in)
{
	//std::cout << "Recevied a STREAM packet from network\n";

	// Is this packet arriving at a rendezvous server?
	if (HandleStreamRawPacket(p_in)) {
		// we handled it, no further processing is needed
		return;
	}
	// INFO("Inside ProcessStreamPacket");
	XIAHeader xiah(p_in->xia_header());
	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();
	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	XID	_source_xid = src_path.xid(src_path.destination_node());

	XIDpair xid_pair;
	xid_pair.set_src(_destination_xid);
	xid_pair.set_dst(_source_xid);
	StreamHeader thdr(p_in);

	// printf("process stream: flags = %08x\n", thdr.flags());

	sock *handler;
	if ((handler = XIDpairToSock.get(xid_pair)) != NULL)
	{
		// INFO("We are in the normal case");
		((XStream *)handler) -> push(p_in);
	} else if ((handler = XIDpairToConnectPending.get(xid_pair)) != NULL)
	{
		// INFO("We are in the second case");
		((XStream *)handler) -> push(p_in);
	}
	else {
		if (thdr.flags() & XTH_SYN) {
			// unlike the other stream handlers, there is no pair yet, so use dest_xid to get port
			sock *sk = XID2Sock(_destination_xid);

			if (!sk) {
				// FIXME: we need to fix the state machine so this doesn't happen!
				WARN("sk == NULL\n");
				return;
			}

			// INFO("socket %d received SYN\n", sk->port);

			if (sk->state != LISTEN) {
				// we aren't marked to accept connecctions, drop it
				WARN("SYN received on a non-listening socket (port:%u), dropping...\n", sk->port);
				return;
			}

			if (sk->pending_connection_buf.size() >= sk->backlog) {
				// the backlog is full, we can't take it right now, drop it

				WARN("SYN received but backlog is full (port:%u), dropping...\n", sk->port);
				return;
			}

			// First, check if this request is already in the pending queue
			HashTable<XIDpair , struct sock*>::iterator it;
			it = XIDpairToConnectPending.find(xid_pair);

			if (it == XIDpairToConnectPending.end()) {
				// if this is new request, put it in the queue

				// send SYNACK to client
				// INFO("Socket %d Handling new SYN\n", sk->port);
				// Prepare new sock for this connection
				XStream *new_sk = new XStream(this, 0); // just for now. This will be updated via Xaccept call
				new_sk->dst_path = src_path;
				new_sk->src_path = dst_path;
				new_sk->listening_sock = sk;
				new_sk->set_key(xid_pair);
				XIDpairToConnectPending.set(xid_pair, new_sk);
				new_sk->push(p_in);
			}
		}
	}


}


bool XTRANSPORT::usingRendezvousDAG(XIAPath bound_dag, XIAPath pkt_dag)
{
	// If both DAGs match, then the pkt_dag did not come through rendezvous service
	if (bound_dag == pkt_dag) {
		return false;
	}
	INFO("DAG possibly modified by a rendezvous server");
	// Find local AD as of now
	XIAPath local_dag = local_addr();
	XID local_ad = local_dag.xid(local_dag.first_ad_node());
	XID bound_ad = bound_dag.xid(bound_dag.first_ad_node());
	XID packet_ad = pkt_dag.xid(pkt_dag.first_ad_node());
	// The local AD must be the same as that in the SYN packet
	if (packet_ad != local_ad) {
		INFO("AD was:%s but local AD is:%s", packet_ad.unparse().c_str(), local_ad.unparse().c_str());
		return false;
	}
	// difference between bound_dag and pkt_dag must be the bound_ad vs. local_ad
	if (bound_dag.compare_with_exception(pkt_dag, bound_ad, local_ad)) {
		ERROR("ERROR: Bound to network:%s", bound_ad.unparse().c_str());
		ERROR("ERROR: Current network:%s", local_ad.unparse().c_str());
		ERROR("ERROR: Wrong AD in packet pkt_dag:%s", pkt_dag.unparse().c_str());
		return false;
	}
	INFO("Allowing DAG different from bound dag");
	return true;
}



// FIXME: look into eliminating the common setup code in all of these function
// pass all as params?
void XTRANSPORT::ProcessMigratePacket(WritablePacket *p_in)
{
	UNUSED(p_in);
#if 0
	XIAHeader xiah(p_in->xia_header());

	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();

	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	XID	_source_xid = src_path.xid(src_path.destination_node());

	TransportHeader thdr(p_in);

	XIDpair xid_pair;
	xid_pair.set_src(_destination_xid);
	xid_pair.set_dst(_source_xid);

	sock *sk = XIDpairToSock.get(xid_pair);

	if (!sk) {
		WARN("ProcessMigratePacket: sk == NULL\n");
		return;
	}

	DBG("ProcessMigrate: %s from port %d at %ld.\n", _source_xid.unparse().c_str(), sk->port, Timestamp::now());

	// Verify the MIGRATE request and start using new DAG
	// No need to wait for an ACK because the operation is idempotent
	// 1. Retrieve payload (srcDAG, destDAG, seqnum) Signature, Pubkey
	const uint8_t *payload = thdr.payload();
	//int payload_len = xiah.plen() - thdr.hlen();
	const uint8_t *payloadptr = payload;
	String remote_DAG((char *)payloadptr, strlen((char *) payloadptr));
	payloadptr += strlen((char *)payloadptr) + 1;
	INFO("MIGRATE: remote DAG: %s", remote_DAG.c_str());
	String my_DAG((char *)payloadptr, strlen((char *) payloadptr));
	payloadptr += strlen((char *)payloadptr) + 1;
	INFO("MIGRATE: my DAG: %s", my_DAG.c_str());
	String timestamp((char *)payloadptr, strlen((char *) payloadptr));
	payloadptr += strlen((char *)payloadptr) + 1;
	INFO("MIGRATE: Timestamp: %s", timestamp.c_str());
	uint16_t siglen;
	memcpy(&siglen, payloadptr, sizeof(uint16_t));
	payloadptr += sizeof(uint16_t);
	INFO("MIGRATE: Signature length: %d", siglen);
	uint8_t *signature = (uint8_t *) calloc(siglen, 1);
	memcpy(signature, payloadptr, siglen);
	payloadptr += siglen;
	uint16_t pubkeylen;
	memcpy(&pubkeylen, payloadptr, sizeof(uint16_t));
	payloadptr += sizeof(uint16_t);
	INFO("MIGRATE: Pubkey length: %d", pubkeylen);
	char *pubkey = (char *) calloc(pubkeylen, 1);
	memcpy(pubkey, payloadptr, pubkeylen);
	INFO("MIGRATE: Pubkey:%s:", pubkey);
	payloadptr += pubkeylen;
	INFO("MIGRATE: pkt len: %d", payloadptr - payload);

	// 2. Verify hash of pubkey matches srcDAG destination node
	if (src_path.parse(remote_DAG) == false) {
		INFO("MIGRATE: ERROR parsing remote DAG:%s:", remote_DAG.c_str());
	}
	String src_SID_string = src_path.xid(src_path.destination_node()).unparse();
	const char *sourceSID = xs_XIDHash(src_SID_string.c_str());
	uint8_t pubkeyhash[SHA_DIGEST_LENGTH];
	char pubkeyhash_hexdigest[XIA_SHA_DIGEST_STR_LEN];
	xs_getPubkeyHash(pubkey, pubkeyhash, sizeof pubkeyhash);
	xs_hexDigest(pubkeyhash, sizeof pubkeyhash, pubkeyhash_hexdigest, sizeof pubkeyhash_hexdigest);
	if (strcmp(pubkeyhash_hexdigest, sourceSID) != 0) {
		INFO("ERROR: MIGRATE pubkey hash: %s SourceSID: %s", pubkeyhash_hexdigest, sourceSID);
	}
	INFO("MIGRATE: Source SID matches pubkey hash");

	// 3. Verify Signature using Pubkey
	size_t signed_datalen = remote_DAG.length() + my_DAG.length() + timestamp.length() + 3;
	if (!xs_isValidSignature(payload, signed_datalen, signature, siglen, pubkey, pubkeylen)) {
		INFO("ProcessNetworkPacket: ERROR: MIGRATE with invalid signature");
	}
	free(signature);
	free(pubkey);
	INFO("MIGRATE: Signature validated");

	// 4. Update socket state dst_path with srcDAG
	sk->dst_path = src_path;
	assert(sk->state == CONNECTED);

	// 5. Return MIGRATEACK to notify mobile host of change
	// Construct the payload - 'data'
	// For now (timestamp) signature, Pubkey
	uint8_t *data;
	uint8_t *dataptr;
	uint32_t maxdatalen;
	uint32_t datalen;
	char mypubkey[MAX_PUBKEY_SIZE];
	uint16_t mypubkeylen = MAX_PUBKEY_SIZE;

	INFO("MIGRATE: building MIGRATEACK");
	XID my_xid = sk->src_path.xid(sk->src_path.destination_node());
	INFO("MIGRATE: MIGRATEACK get pubkey for:%s:", my_xid.unparse().c_str());
	if (xs_getPubkey(my_xid.unparse().c_str(), mypubkey, &mypubkeylen)) {
		ERROR("ERROR: getting Pubkey for MIGRATEACK");
	}
	maxdatalen = remote_DAG.length() + 1 + timestamp.length() + 1 + MAX_SIGNATURE_SIZE + sizeof(uint16_t) + mypubkeylen;
	data = (uint8_t *) calloc(maxdatalen, 1);
	if (data == NULL) {
		ERROR("ERROR allocating memory for MIGRATEACK");
	}
	dataptr = data;

	// Insert the mobile host DAG whose migration has been accepted
	strcpy((char *)dataptr, remote_DAG.c_str());
	INFO("MIGRATE: MIGRATEACK remoteDAG: %s", (char *)dataptr);
	dataptr += remote_DAG.length() + 1; // null-terminated string

	// Insert timestamp into payload
	strcpy((char *)dataptr, timestamp.c_str());
	INFO("MIGRATE: MIGRATEACK timestamp: %s", (char *)dataptr);
	dataptr += timestamp.length() + 1; // null-terminated string

	// Sign(mobileDAG, Timestamp)
	uint8_t mysignature[MAX_SIGNATURE_SIZE];
	uint16_t mysiglen = MAX_SIGNATURE_SIZE;
	if (xs_sign(my_xid.unparse().c_str(), data, dataptr - data, mysignature, &mysiglen)) {
		ERROR("ERROR signing MIGRATEACK");
	}

	// Signature length
	memcpy(dataptr, &mysiglen, sizeof(uint16_t));
	INFO("MIGRATE: MIGRATEACK siglen: %d", mysiglen);
	dataptr += sizeof(uint16_t);

	// Signature
	memcpy(dataptr, mysignature, mysiglen);
	dataptr += mysiglen;

	// Public key length
	memcpy(dataptr, &mypubkeylen, sizeof(uint16_t));
	INFO("MIGRATE: MIGRATEACK pubkeylen: %d", mypubkeylen);
	dataptr += sizeof(uint16_t);

	// Public key
	memcpy(dataptr, mypubkey, mypubkeylen);
	INFO("MIGRATE: MIGRATEACK pubkey:%s:", dataptr);
	dataptr += mypubkeylen;

	// Total payload length
	datalen = dataptr - data;
	INFO("MIGRATE: MIGRATEACK len: %d", datalen);

	// Create a packet with the payload
	XIAHeaderEncap xiah_new;
	xiah_new.set_nxt(CLICK_XIA_NXT_TRN);
	xiah_new.set_last(LAST_NODE_DEFAULT);
	xiah_new.set_hlim(HLIM_DEFAULT);
	xiah_new.set_dst_path(src_path);
	xiah_new.set_src_path(dst_path);

	WritablePacket *just_payload_part = WritablePacket::make(256, data, datalen, 0);
	free(data);

	WritablePacket *p = NULL;

	xiah_new.set_plen(datalen);

	TransportHeaderEncap *thdr_new = TransportHeaderEncap::MakeMIGRATEACKHeader( 0, 0, 0, calc_recv_window(sk)); // #seq, #ack, length
	p = thdr_new->encap(just_payload_part);

	thdr_new->update();
	xiah_new.set_plen(datalen + thdr_new->hlen()); // XIA payload = transport header + transport-layer data

	p = xiah_new.encap(p, false);

	delete thdr_new;
	output(NETWORK_PORT).push(p);

	// 6. Notify the api of MIGRATE reception
	//   Do we need to? -Nitin
#endif
}



void XTRANSPORT::ProcessMigrateAck(WritablePacket *p_in)
{
	UNUSED(p_in);
/*
	XIAHeader xiah(p_in->xia_header());

	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();

	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	XID	_source_xid = src_path.xid(src_path.destination_node());

	TransportHeader thdr(p_in);

	XIDpair xid_pair;
	xid_pair.set_src(_destination_xid);
	xid_pair.set_dst(_source_xid);

	sock *sk = XIDpairToSock.get(xid_pair);
	if (!sk) {
		ERROR("sk == NULL\n");
		return;
	}
	unsigned short _dport = sk->port;

	if (sk->state != CONNECTED) {
		// This should never happen!
		ERROR("socket is not connected\n");
		return;
	}

	DBG("%s from port %d at %ld.\n", _source_xid.unparse().c_str(), _dport, Timestamp::now());

	// Verify the MIGRATEACK and start using new DAG
	// 1. Retrieve payload (migratedDAG, timestamp) signature, Pubkey
	const uint8_t *payload = thdr.payload();
	int payload_len = xiah.plen() - thdr.hlen();
	const uint8_t *payloadptr = payload;
	size_t signed_datalen;

	// Extract the migrated DAG that the fixed host accepted
	String migrated_DAG((char *)payloadptr, strlen((char *) payloadptr));
	payloadptr += strlen((char *)payloadptr) + 1;
	INFO("MIGRATEACK: migrated DAG: %s", migrated_DAG.c_str());

	// Extract the timestamp corresponding to the migration message that was sent
	// Helps handle a second migration before the first migration is completed
	String timestamp((char *)payloadptr, strlen((char *) payloadptr));
	payloadptr += strlen((char *)payloadptr) + 1;
	signed_datalen = payloadptr - payload;
	INFO("MIGRATEACK: timestamp: %s", timestamp.c_str());

	// Get the signature (migrated_DAG, timestamp)
	uint16_t siglen;
	memcpy(&siglen, payloadptr, sizeof(uint16_t));
	INFO("MIGRATEACK: siglen: %d", siglen);
	payloadptr += sizeof(uint16_t);
	uint8_t *signature = (uint8_t *) calloc(siglen, 1);
	memcpy(signature, payloadptr, siglen);
	payloadptr += siglen;

	// Get the Public key of the fixed host
	uint16_t pubkeylen;
	memcpy(&pubkeylen, payloadptr, sizeof(uint16_t));
	INFO("MIGRATEACK: pubkeylen: %d", pubkeylen);
	payloadptr += sizeof(uint16_t);
	char *pubkey = (char *) calloc(pubkeylen, 1);
	memcpy(pubkey, payloadptr, pubkeylen);
	INFO("MIGRATEACK: pubkey:%s:", pubkey);
	payloadptr += pubkeylen;
	if (payloadptr - payload != payload_len) {
		WARN("MIGRATEACK expected payload len=%d, got %d", payload_len, payloadptr - payload);
	}
	//assert(payloadptr-payload == payload_len);

	// 2. Verify hash of pubkey matches the fixed host's SID
	String fixed_SID_string = sk->dst_path.xid(sk->dst_path.destination_node()).unparse();
	uint8_t pubkeyhash[SHA_DIGEST_LENGTH];
	char pubkeyhash_hexdigest[XIA_SHA_DIGEST_STR_LEN];
	xs_getPubkeyHash(pubkey, pubkeyhash, sizeof pubkeyhash);
	xs_hexDigest(pubkeyhash, sizeof pubkeyhash, pubkeyhash_hexdigest, sizeof pubkeyhash_hexdigest);
	if (strcmp(pubkeyhash_hexdigest, xs_XIDHash(fixed_SID_string.c_str())) != 0) {
		ERROR("ERROR: MIGRATEACK: Mismatch: fixedSID: %s, pubkeyhash: %s", fixed_SID_string.c_str(), pubkeyhash_hexdigest);
	}
	INFO("Hash of pubkey matches fixed SID");

	// 3. Verify Signature using Pubkey
	if (!xs_isValidSignature(payload, signed_datalen, signature, siglen, pubkey, pubkeylen)) {
		ERROR("ERROR: MIGRATEACK: MIGRATE with invalid signature");
	}
	INFO("MIGRATEACK: Signature verified");
	free(signature);
	free(pubkey);

	// 4. Verify timestamp matches the latest migrate message
	if (strcmp(sk->last_migrate_ts.c_str(), timestamp.c_str()) != 0) {
		WARN("timestamp sent:%s:, migrateack has:%s:", sk->last_migrate_ts.c_str(), timestamp.c_str());
	}
	INFO("MIGRATEACK: verified timestamp");

	// 5. Update socket state src_path to use the new DAG
	// TODO: Verify migrated_DAG's destination node is the same as src_path's
	//	   before replacing with the migrated_DAG
	sk->src_path.parse(migrated_DAG);
	INFO("MIGRATEACK: updated sock state with newly acknowledged DAG");

	// 6. The data retransmissions can now resume
	sk->migrateack_waiting = false;
	sk->num_migrate_tries = 0;

	portToSock.set(_dport, sk);
	if (_dport != sk->port) {
		INFO("MIGRATEACK: ERROR _dport %d, sk->port %d", _dport, sk->port);
	}
*/
}



void XTRANSPORT::ProcessXcmpPacket(WritablePacket *p_in)
{
	XIAHeader xiah(p_in->xia_header());

	String src_path = xiah.src_path().unparse();
	String header((const char*)xiah.hdr(), xiah.hdr_size());
	String payload((const char*)xiah.payload(), xiah.plen());
	String str = header + payload;

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XRECV);
	xia::X_Recvfrom_Msg *x_recvfrom_msg = xsm.mutable_x_recvfrom();
	x_recvfrom_msg->set_sender_dag(src_path.c_str());
	x_recvfrom_msg->set_payload(str.c_str(), str.length());

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	list<int>::iterator i;

	for (i = xcmp_listeners.begin(); i != xcmp_listeners.end(); i++) {
		int port = *i;

		sock *sk = portToSock.get(port);

		if (sk && sk->sock_type == SOCK_RAW && should_buffer_received_packet(p_in, sk)) {
			add_packet_to_recv_buf(p_in, sk);

			if (sk->polling) {
				// tell API we are readable
				ProcessPollEvent(port, POLLIN);
			}
			check_for_and_handle_pending_recv(sk);
		}
	}
}



void XTRANSPORT::MigrateFailure(sock *sk)
{
	if (sk->polling) {
		// tell API we are in trouble
		ProcessPollEvent(sk->port, POLLHUP);
	}

	if (sk->isBlocking && sk->recv_pending) {
		// The api is blocking on a recv, return an error
		ReturnResult(sk->port, sk->pending_recv_msg, -1, ESTALE);

		sk->recv_pending = false;
		delete sk->pending_recv_msg;
		sk->pending_recv_msg = NULL;
	}
}

sock *XTRANSPORT::XID2Sock(XID dest_xid)
{
	sock *sk = XIDtoSock.get(dest_xid);

	if (sk)
		return sk;

	if (ntohl(dest_xid.type()) == CLICK_XIA_XID_TYPE_CID) {
		std::cout << "Searching for xcacheSID\n";
		// Packet destined to a CID. Handling it specially.
		// FIXME: This is hackish. Maybe give users the ability to
		// register their own rules?

		return XIDtoSock.get(_xcache_sid);
	}

	return NULL;
}


XIAPath XTRANSPORT::alterCIDDstPath(XIAPath dstPath)
{
	// FIXME: If the address is not altered, there's a chance that CID packets
	// of a live download may get diverted to the origin server who does not
	// know about it
#if 0
	XID CID(dstPath.xid(dstPath.destination_node()));
	XIAPath newPath;
	String dagString(_local_addr.unparse().c_str());
	dagString += " ";
	dagString += CID.unparse().c_str();

	std::cout << "FORMED: " << dagString.c_str() << "\n";
	newPath.parse(dagString);
#endif
	return dstPath;
}

#if 0
void XTRANSPORT::ProcessSynPacket(WritablePacket *p_in)
{
	XIAHeader xiah(p_in->xia_header());

	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();

	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	XID	_source_xid = src_path.xid(src_path.destination_node());

	TransportHeader thdr(p_in);

	XIDpair xid_pair;
	xid_pair.set_src(_destination_xid);
	xid_pair.set_dst(_source_xid);

	// unlike the other stream handlers, there is no pair yet, so use dest_xid to get port
	sock *sk = XID2Sock(_destination_xid);

	if (!sk) {
		// FIXME: we need to fix the state machine so this doesn't happen!
		WARN("sk == NULL\n");
		return;
	}

	INFO("socket %d received SYN\n", sk->port);

	if (sk->state != LISTEN) {
		// we aren't marked to accept connecctions, drop it
		WARN("SYN received on a non-listening socket (port:%u), dropping...\n", sk->port);
		return;
	}

	if (sk->pending_connection_buf.size() >= sk->backlog) {
		// the backlog is full, we can't take it right now, drop it

		WARN("SYN received but backlog is full (port:%u), dropping...\n", sk->port);
		return;
	}

	// First, check if this request is already in the pending queue
	HashTable<XIDpair , struct sock*>::iterator it;
	it = XIDpairToConnectPending.find(xid_pair);

	if (it != XIDpairToConnectPending.end()) {
		// we've already seen it, ignore it
		INFO("Socket %d received duplicate SYN\n", sk->port);

		// FIXME: is this OK?
		it->second->num_retransmits = 0;
		return ;
	}

	// For a CID packet, modify dst_path variable
	dst_path = alterCIDDstPath(dst_path);

	// if this is new request, put it in the queue

	// send SYNACK to client
	INFO("Socket %d Handling new SYN\n", sk->port);

	XIAHeaderEncap xiah_new;
	xiah_new.set_nxt(CLICK_XIA_NXT_TRN);
	xiah_new.set_last(LAST_NODE_DEFAULT);
	xiah_new.set_hlim(HLIM_DEFAULT);
	xiah_new.set_dst_path(src_path);
	xiah_new.set_src_path(dst_path);

	WritablePacket *just_payload_part;
	int payloadLength;

	// FIXME: use SendControlPacket to send the SYNACK instead of building it by hand

	std::cout << "Xcachesock = " <<sk->xcacheSock << "\n";
	// FIXME: move to a separate function
	if(!sk->xcacheSock && usingRendezvousDAG(sk->src_path, dst_path)) {
		XID _destination_xid = dst_path.xid(dst_path.destination_node());
		INFO("Sending SYNACK with verification for RV DAG");
		// Destination DAG from the SYN packet
		String src_path_str = dst_path.unparse();

		// Current timestamp as nonce against replay attacks
		Timestamp now = Timestamp::now();
		double timestamp = strtod(now.unparse().c_str(), NULL);

		// Build the payload with DAG for this service and timestamp
		XIASecurityBuffer synackPayload(1024);
		synackPayload.pack(src_path_str.c_str(), src_path_str.length());
		synackPayload.pack((const char *)&timestamp, (uint16_t) sizeof timestamp);

		// Sign the synack payload
		char signature[MAX_SIGNATURE_SIZE];
		uint16_t signatureLength = MAX_SIGNATURE_SIZE;
		if(xs_sign(_destination_xid.unparse().c_str(), (unsigned char *)synackPayload.get_buffer(), synackPayload.size(), (unsigned char *)signature, &signatureLength)) {
			ERROR("ERROR unable to sign the SYNACK using private key for %s", _destination_xid.unparse().c_str());
			MigrateFailure(sk);
			return;
		}

		// Retrieve public key for this host
		char pubkey[MAX_PUBKEY_SIZE];
		uint16_t pubkeyLength = MAX_PUBKEY_SIZE;
		if(xs_getPubkey(_destination_xid.unparse().c_str(), pubkey, &pubkeyLength)) {
			ERROR("ERROR public key not found for %s", _destination_xid.unparse().c_str());
			MigrateFailure(sk);
			return;
		}

		// Prepare a signed payload (serviceDAG, timestamp)Signature, Pubkey
		XIASecurityBuffer signedPayload(2048);
		signedPayload.pack(synackPayload.get_buffer(), synackPayload.size());
		signedPayload.pack(signature, signatureLength);
		signedPayload.pack((char *)pubkey, pubkeyLength);

		just_payload_part = WritablePacket::make(256, (const void*)signedPayload.get_buffer(), signedPayload.size(), 1);
		payloadLength = signedPayload.size();
	} else {
		const char* dummy = "Connection_pending";
		just_payload_part = WritablePacket::make(256, dummy, strlen(dummy), 0);
		payloadLength = strlen(dummy);
	}

	WritablePacket *p = NULL;

	xiah_new.set_plen(payloadLength);

	TransportHeaderEncap *thdr_new = TransportHeaderEncap::MakeSYNACKHeader(0, 0, 0, calc_recv_window(sk)); // #seq, #ack, length, recv_wind
	p = thdr_new->encap(just_payload_part);

	thdr_new->update();
	xiah_new.set_plen(payloadLength + thdr_new->hlen()); // XIA payload = transport header + transport-layer data

	p = xiah_new.encap(p, false);
	delete thdr_new;

	// Prepare new sock for this connection
	sock *new_sk = new sock();
	ChangeState(new_sk, SYN_RCVD);
	new_sk->port = 0; // just for now. This will be updated via Xaccept call
	new_sk->sock_type = SOCK_STREAM;
	new_sk->dst_path = src_path;
	new_sk->src_path = dst_path;
	new_sk->isAcceptedSocket = true;
	new_sk->pkt = copy_packet(p, new_sk);
		new_sk->refcount = 1;

	memset(new_sk->send_buffer, 0, new_sk->send_buffer_size * sizeof(WritablePacket*));
	memset(new_sk->recv_buffer, 0, new_sk->recv_buffer_size * sizeof(WritablePacket*));

	ScheduleTimer(new_sk, ACK_DELAY);

	XIDpairToConnectPending.set(xid_pair, new_sk);

	output(NETWORK_PORT).push(p);
	INFO("Sent SYNACK from new socket\n");
}
#endif


int XTRANSPORT::HandleStreamRawPacket(WritablePacket *p_in)
{
	XIAHeader xiah(p_in->xia_header());

	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();

	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	XID	_source_xid = src_path.xid(src_path.destination_node());

	StreamHeader thdr(p_in);

	XIDpair xid_pair;
	xid_pair.set_src(_destination_xid);
	xid_pair.set_dst(_source_xid);

	sock *sk = XID2Sock(_destination_xid);

	if (!sk) {
		ERROR("sk == NULL\n");
		return 0;
	}
	unsigned short _dport = sk->port;

	// it's not a raw packet, so tell ProcessNetworkPacket to handle it
	if (sk->sock_type != SOCK_RAW) {
		return 0;
	}

	if (!should_buffer_received_packet(p_in, sk)) {
		return 1;
	}
	INFO("socket %d STATE:%s\n", sk->port, StateStr(sk->state));

	String src_path_str = src_path.unparse();
	String dst_path_str = dst_path.unparse();
	INFO("received stream packet on raw socket");
	INFO("src|%s|", src_path_str.c_str());
	INFO("dst|%s|", dst_path_str.c_str());
	INFO("len=%d", p_in->length());

	add_packet_to_recv_buf(p_in, sk);
	if (sk->polling) {
		// tell API we are readable
		ProcessPollEvent(_dport, POLLIN);
	}
	check_for_and_handle_pending_recv(sk);
	return 1;
}

/*************************************************************
** CHUNK PACKET HANDLERS
*************************************************************/
// for response pkt, verify it, clear the reqPkt entry in the socket, send it to upper layer if read_cid_req is true, or if store it in XIDtoCIDresponsePkt
void XTRANSPORT::ProcessCachePacket(WritablePacket *p_in)
{
	UNUSED(p_in);
#if 0
	DBG("Got packet from cache");

	//Extract the SID/CID
	XIAHeader xiah(p_in->xia_header());
	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();
	XID	destination_sid = dst_path.xid(dst_path.destination_node());
	XID	source_cid = src_path.xid(src_path.destination_node());

	ContentHeader ch(p_in);

	if (ch.opcode() == ContentHeader::OP_PUSH) {
		// compute the hash and verify it matches the CID
		unsigned char digest[SHA_DIGEST_LENGTH];
		xs_getSHA1Hash((const unsigned char *)xiah.payload(), xiah.plen(), \
			digest, SHA_DIGEST_LENGTH);

		String hash = "CID:";
		char hexBuf[3];
		for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
			sprintf(hexBuf, "%02x", digest[i]);
			hash.append(const_cast<char *>(hexBuf), 2);
		}

		// int status = READY_TO_READ;
		if (hash != source_cid.unparse()) {
			WARN("CID with invalid hash received: %s\n", source_cid.unparse().c_str());
			// status = INVALID_HASH;
		}

		unsigned short _dport = XIDtoPushPort.get(destination_sid);
		if (!_dport) {
			WARN("Couldn't find SID to send to: %s\n", destination_sid.unparse().c_str());
			return;
		}

		sock *sk = portToSock.get(_dport);
		if (sk->sock_type != SOCK_CHUNK) {
			WARN("This is not a chunk socket. dport: %i, Socktype: %i", _dport, sk->sock_type);
		}

		// Send pkt up

		//Unparse dag info
		String src_path = xiah.src_path().unparse();

		// FIXMEFIXMEFIXME
		xia::XSocketMsg xia_socket_msg;
		xia_socket_msg.set_type(xia::XRECVCHUNKFROM);
		xia::X_Recvchunkfrom_Msg *x_recvchunkfrom_msg = xia_socket_msg.mutable_x_recvchunkfrom();
		x_recvchunkfrom_msg->set_cid(source_cid.unparse().c_str());
		x_recvchunkfrom_msg->set_payload((const char*)xiah.payload(), xiah.plen());
		x_recvchunkfrom_msg->set_cachepolicy(ch.cachePolicy());
		x_recvchunkfrom_msg->set_ttl(ch.ttl());
		x_recvchunkfrom_msg->set_cachesize(ch.cacheSize());
		x_recvchunkfrom_msg->set_contextid(ch.contextID());
		x_recvchunkfrom_msg->set_length(ch.length());
		x_recvchunkfrom_msg->set_ddag(dst_path.unparse().c_str());

		std::string p_buf;
		xia_socket_msg.SerializeToString(&p_buf);

		WritablePacket *p2 = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

		DBG("Sent packet to socket: sport %d dport %d", _dport, _dport);

		output(API_PORT).push(UDPIPPrep(p2, _dport));
		return;
	}

	XIDpair xid_pair;
	xid_pair.set_src(destination_sid);
	xid_pair.set_dst(source_cid);

	sock *sk = XIDpairToSock.get(xid_pair);
	unsigned short _dport = sk->port;

	INFO("CachePacket, dest: %s, src_cid %s OPCode: %d \n", destination_sid.unparse().c_str(), source_cid.unparse().c_str(), ch.opcode());

	if (_dport) {
		//TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
		//sock sk=portToSock.get(_dport);
		//sk.dst_path=xiah.src_path();
		//portToSock.set(_dport,sk);
		//ENDTODO

		// Reset timer or just Remove the corresponding entry in the hash tables (Done below)
		HashTable<XID, WritablePacket*>::iterator it1;
		it1 = sk->XIDtoCIDreqPkt.find(source_cid);

		if (it1 != sk->XIDtoCIDreqPkt.end() ) {
			// Remove the entry
			sk->XIDtoCIDreqPkt.erase(it1);
		}

		HashTable<XID, Timestamp>::iterator it2;
		it2 = sk->XIDtoExpiryTime.find(source_cid);

		if (it2 != sk->XIDtoExpiryTime.end()) {
			// Remove the entry
			sk->XIDtoExpiryTime.erase(it2);
		}

		HashTable<XID, bool>::iterator it3;
		it3 = sk->XIDtoTimerOn.find(source_cid);

		if (it3 != sk->XIDtoTimerOn.end()) {
			// Remove the entry
			sk->XIDtoTimerOn.erase(it3);
		}

		// compute the hash and verify it matches the CID
		unsigned char digest[SHA_DIGEST_LENGTH];
		xs_getSHA1Hash((const unsigned char *)xiah.payload(), xiah.plen(), \
			digest, SHA_DIGEST_LENGTH);

		String hash = "CID:";
		char hexBuf[3];
		for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
			sprintf(hexBuf, "%02x", digest[i]);
			hash.append(const_cast<char *>(hexBuf), 2);
		}

		int status = READY_TO_READ;
		if (hash != source_cid.unparse()) {
			WARN("CID with invalid hash received: %s\n", source_cid.unparse().c_str());
			status = INVALID_HASH;
		}

		// Update the status of CID request
		sk->XIDtoStatus.set(source_cid, status);

		// Check if the ReadCID() was called for this CID
		HashTable<XID, bool>::iterator it4;
		it4 = sk->XIDtoReadReq.find(source_cid);

		if (it4 != sk->XIDtoReadReq.end()) {
			// There is an entry
			bool read_cid_req = it4->second;

			if (read_cid_req == true) {
				// Send pkt up
				sk->XIDtoReadReq.erase(it4);

				portToSock.set(_dport, sk);
				if (_dport != sk->port) {
					ERROR("ERROR _dport %d, sk->port %d", _dport, sk->port);
				}

				//Unparse dag info
				String src_path = xiah.src_path().unparse();

				xia::XSocketMsg xia_socket_msg;
				xia_socket_msg.set_type(xia::XREADCHUNK);
				xia::X_Readchunk_Msg *x_readchunk_msg = xia_socket_msg.mutable_x_readchunk();
				x_readchunk_msg->set_dag(src_path.c_str());
				x_readchunk_msg->set_payload((const char*)xiah.payload(), xiah.plen());

				std::string p_buf;
				xia_socket_msg.SerializeToString(&p_buf);

				WritablePacket *p2 = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

				DBG("Sent packet to socket: sport %d dport %d \n", _dport, _dport);

				output(API_PORT).push(UDPIPPrep(p2, _dport));

			} else {
				// Store the packet into temp buffer (until ReadCID() is called for this CID)
				WritablePacket *copy_response_pkt = copy_cid_response_packet(p_in, sk);
				sk->XIDtoCIDresponsePkt.set(source_cid, copy_response_pkt);

				portToSock.set(_dport, sk);
				if (_dport != sk->port) {
					ERROR("ERROR _dport %d, sk->port %d", _dport, sk->port);
				}
			}

		} else {
			WritablePacket *copy_response_pkt = copy_cid_response_packet(p_in, sk);
			sk->XIDtoCIDresponsePkt.set(source_cid, copy_response_pkt);
			portToSock.set(_dport, sk);
			if (_dport != sk->port) {
				ERROR("ERROR _dport %d, sk->port %d", _dport, sk->port);
			}
		}
	}
	else
	{
		WARN("Packet to unknown %s, src_cid %s\n", destination_sid.unparse().c_str(), source_cid.unparse().c_str());
	}
#endif
}



/*************************************************************
** API FACING FUNCTIONS
*************************************************************/
void XTRANSPORT::ProcessAPIPacket(WritablePacket *p_in)
{
	//Extract the destination port
	unsigned short _sport = SRC_PORT_ANNO(p_in);

	// DBG("Push: Got packet from API sport:%d",ntohs(_sport));

	std::string p_buf;
	p_buf.assign((const char*)p_in->data(), (const char*)p_in->end_data());

	//protobuf message parsing
	xia::XSocketMsg xia_socket_msg;
	xia_socket_msg.ParseFromString(p_buf);
	switch (xia_socket_msg.type()) {
	case xia::XSOCKET:
		Xsocket(_sport, &xia_socket_msg);
		break;
	case xia::XSETSOCKOPT:
		Xsetsockopt(_sport, &xia_socket_msg);
		break;
	case xia::XGETSOCKOPT:
		Xgetsockopt(_sport, &xia_socket_msg);
		break;
	case xia::XBIND:
		Xbind(_sport, &xia_socket_msg);
		break;
	case xia::XCLOSE:
		Xclose(_sport, &xia_socket_msg);
		break;
	case xia::XCONNECT:
		Xconnect(_sport, &xia_socket_msg);
		break;
	case xia::XLISTEN:
		Xlisten(_sport, &xia_socket_msg);
		break;
	case xia::XREADYTOACCEPT:
		XreadyToAccept(_sport, &xia_socket_msg);
		break;
	case xia::XACCEPT:
		Xaccept(_sport, &xia_socket_msg);
		break;
	case xia::XCHANGEAD:
		Xchangead(_sport, &xia_socket_msg);
		break;
	case xia::XREADLOCALHOSTADDR:
		Xreadlocalhostaddr(_sport, &xia_socket_msg);
		break;
	case xia::XSETXCACHESID:
		XsetXcacheSid(_sport, &xia_socket_msg);
		break;
	case xia::XUPDATENAMESERVERDAG:
		Xupdatenameserverdag(_sport, &xia_socket_msg);
		break;
	case xia::XREADNAMESERVERDAG:
		Xreadnameserverdag(_sport, &xia_socket_msg);
		break;
	case xia::XISDUALSTACKROUTER:
		Xisdualstackrouter(_sport, &xia_socket_msg);
		break;
	case xia::XSEND:
		Xsend(_sport, &xia_socket_msg, p_in);
		break;
	case xia::XSENDTO:
		Xsendto(_sport, &xia_socket_msg, p_in);
		break;
	case xia::XRECV:
		Xrecv(_sport, &xia_socket_msg);
		break;
	case xia::XRECVFROM:
		Xrecvfrom(_sport, &xia_socket_msg);
		break;
#if 0
	case xia::XREQUESTCHUNK:
		XrequestChunk(_sport, &xia_socket_msg, p_in);
		break;
	case xia::XGETCHUNKSTATUS:
		XgetChunkStatus(_sport, &xia_socket_msg);
		break;
	case xia::XREADCHUNK:
		XreadChunk(_sport, &xia_socket_msg);
		break;
	case xia::XREMOVECHUNK:
		XremoveChunk(_sport, &xia_socket_msg);
		break;
	case xia::XPUTCHUNK:
		XputChunk(_sport, &xia_socket_msg);
		break;
#endif
	case xia::XGETPEERNAME:
		Xgetpeername(_sport, &xia_socket_msg);
		break;
	case xia::XGETSOCKNAME:
		Xgetsockname(_sport, &xia_socket_msg);
		break;
	case xia::XPOLL:
		Xpoll(_sport, &xia_socket_msg);
		break;
#if 0
	case xia::XPUSHCHUNKTO:
		XpushChunkto(_sport, &xia_socket_msg, p_in);
		break;
	case xia::XBINDPUSH:
		XbindPush(_sport, &xia_socket_msg);
		break;
#endif
	case xia::XUPDATERV:
		Xupdaterv(_sport, &xia_socket_msg);
		break;
	case xia::XFORK:
		Xfork(_sport, &xia_socket_msg);
		break;
	case xia::XREPLAY:
		Xreplay(_sport, &xia_socket_msg);
		break;
	case xia::XNOTIFY:
		Xnotify(_sport, &xia_socket_msg);
		break;
	default:
		ERROR("ERROR: Unknown API request\n");
		break;
	}

	p_in->kill();
}



void XTRANSPORT::ReturnResult(int sport, xia::XSocketMsg *xia_socket_msg, int rc, int err)
{
	xia::X_Result_Msg *x_result = xia_socket_msg->mutable_x_result();
	x_result->set_return_code(rc);
	x_result->set_err_code(err);

	std::string p_buf;
	xia_socket_msg->SerializeToString(&p_buf);
	WritablePacket *reply = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, sport));
}



/*
** Handler for the Xsocket API call
*/
void XTRANSPORT::Xsocket(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Socket_Msg *x_socket_msg = xia_socket_msg->mutable_x_socket();
	int sock_type = x_socket_msg->type();

	DBG("create %s socket %d\n", SocketTypeStr(sock_type), _sport);
	sock *sk = NULL;
	switch (sock_type) {
	case SOCK_STREAM: {
		sk = new XStream(this, _sport);
		break;
	}
	case SOCK_DGRAM: {
		sk = new XDatagram(this, _sport);
		break;
	}
//	case SOCK_CHUNK: {
//		sk = new XChunk(this, _sport);
//		break;
//	}
	}

	// Map the source port to sock
	portToSock.set(_sport, sk);

	// Return result to API
	ReturnResult(_sport, xia_socket_msg, 0);
}

/*
** Xsetsockopt API handler
*/
void XTRANSPORT::Xsetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Setsockopt_Msg *x_sso_msg = xia_socket_msg->mutable_x_setsockopt();
	sock *sk = portToSock.get(_sport);

	switch (x_sso_msg->opt_type()) {
	case XOPT_HLIM:
	{
		int hl = x_sso_msg->int_opt();

		sk->hlim = hl;
	}
	break;

	case XOPT_NEXT_PROTO:
	{
		int nxt = x_sso_msg->int_opt();
		sk->nxt_xport = nxt;
		if (nxt == CLICK_XIA_NXT_XCMP)
			xcmp_listeners.push_back(_sport);
		else
			xcmp_listeners.remove(_sport);
	}
	break;

	case XOPT_BLOCK:
		sk->isBlocking = x_sso_msg->int_opt();
		break;

	case SO_DEBUG:
		sk->so_debug = x_sso_msg->int_opt();
		break;

		case SO_ERROR:
			sk->so_error = x_sso_msg->int_opt();
			break;

	default:
		// unsupported option
		break;
	}

	ReturnResult(_sport, xia_socket_msg);
}

/*
** Xgetsockopt API handler
*/
void XTRANSPORT::Xgetsockopt(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Getsockopt_Msg *x_sso_msg = xia_socket_msg->mutable_x_getsockopt();

	sock *sk = portToSock.get(_sport);

	switch (x_sso_msg->opt_type()) {
	case XOPT_HLIM:
		x_sso_msg->set_int_opt(sk->hlim);
		break;

	case XOPT_NEXT_PROTO:
		x_sso_msg->set_int_opt(sk->nxt_xport);
		break;

	case SO_ACCEPTCONN:
		x_sso_msg->set_int_opt(sk->state == LISTEN);
		break;

	case SO_DEBUG:
		x_sso_msg->set_int_opt(sk->so_debug);
		break;

	case SO_ERROR:
		x_sso_msg->set_int_opt(sk->so_error);
		sk->so_error = 0;
		break;

	case XOPT_ERROR_PEEK:
		// same as SO_ERROR, but doesn't reset the error code
		x_sso_msg->set_int_opt(sk->so_error);
		break;

	default:
		// unsupported option
		break;
	}

	ReturnResult(_sport, xia_socket_msg);
}

void XTRANSPORT::Xbind(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	int rc = 0, ec = 0;

	xia::X_Bind_Msg *x_bind_msg = xia_socket_msg->mutable_x_bind();

	String sdag_string(x_bind_msg->sdag().c_str(), x_bind_msg->sdag().size());

	//Set the source DAG in sock
	sock *sk = portToSock.get(_sport);
	if (sk->src_path.parse(sdag_string)) {
		sk->initialized = true;
		sk->port = _sport;

		//Check if binding to full DAG or just to SID only
		Vector<XIAPath::handle_t> xids = sk->src_path.next_nodes( sk->src_path.source_node() );
		XID front_xid = sk->src_path.xid( xids[0] );
		struct click_xia_xid head_xid = front_xid.xid();
		uint32_t head_xid_type = head_xid.type;
		if (head_xid_type == _sid_type) {
			sk->full_src_dag = false;
		} else {
			sk->full_src_dag = true;
		}

		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
		//TODO: Add a check to see if XID is already being used

		// Map the source XID to source port (for now, for either type of tranports)
		XIDtoSock.set(source_xid, sk);
		if(source_xid == _xcache_sid) {
			sk->xcacheSock = true;
		} else {
			sk->xcacheSock = false;
		}
		addRoute(source_xid);
		portToSock.set(_sport, sk);
		if (_sport != sk->port) {
			ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
		}
	} else {
		rc = -1;
		ec = EADDRNOTAVAIL;
	}

	ReturnResult(_sport, xia_socket_msg, rc, ec);
}



void XTRANSPORT::Xfork(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Fork_Msg *msg = xia_socket_msg->mutable_x_fork();
	int count = msg->count();
	int increment = msg->increment() ? 1 : -1;

//	xia_socket_msg->PrintDebugString();

	// loop through list of ports and modify the ref counter
	for (int i = 0; i < count; i++) {
		int port = msg->ports(i);

		DBG("port = %d\n", port);

		sock *sk = portToSock.get(port);
		if (sk) {
			sk->refcount += increment;
			DBG("%s refcount for %d (%d)\n", (increment > 0 ? "incrementing" : "decrementing"), port, sk->refcount);
			assert(sk->refcount > 0);
		}
	}

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xreplay(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Replay_Msg *msg = xia_socket_msg->mutable_x_replay();

	DBG("Received REPLAY packet\n");
//	xia_socket_msg->PrintDebugString();

	xia_socket_msg->set_type(msg->type());
	xia_socket_msg->set_sequence(msg->sequence());

	ReturnResult(_sport, xia_socket_msg);
}


void XTRANSPORT::Xnotify(unsigned short _sport, xia::XSocketMsg * /*xia_socket_msg */)
{
	notify_listeners.push_back(_sport);

	// we just go away and wait for XchangeAD to be called which will trigger a response on this client socket
}


#if 0
// FIXME: This way of doing things is a bit hacky.
void XTRANSPORT::XbindPush(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	int rc = 0, ec = 0;

	xia::X_BindPush_Msg *x_bindpush_msg = xia_socket_msg->mutable_x_bindpush();

	String sdag_string(x_bindpush_msg->sdag().c_str(), x_bindpush_msg->sdag().size());

	DBG("\nbind requested to %s\n", sdag_string.c_str());

	//Set the source DAG in sock
	sock *sk = portToSock.get(_sport);
	if (sk->src_path.parse(sdag_string)) {
		ChangeState(sk, INACTIVE);
		sk->initialized = true;

		//Check if binding to full DAG or just to SID only
		Vector<XIAPath::handle_t> xids = sk->src_path.next_nodes( sk->src_path.source_node() );
		XID front_xid = sk->src_path.xid( xids[0] );

		struct click_xia_xid head_xid = front_xid.xid();
		uint32_t head_xid_type = head_xid.type;
		if (head_xid_type == _sid_type) {
			sk->full_src_dag = false;
		} else {
			sk->full_src_dag = true;
		}

		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
		//TODO: Add a check to see if XID is already being used

		// Map the source XID to source port (for now, for either type of tranports)
		XIDtoPushPort.set(source_xid, _sport);
		addRoute(source_xid);

		portToSock.set(_sport, sk);
		if (_sport != sk->port) {
			ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
		}

	} else {
		rc = -1;
		ec = EADDRNOTAVAIL;
		ERROR("ERROR: SOCKET PUSH BIND !!!\\n");
	}

	// (for Ack purpose) Reply with a packet with the destination port=source port
	ReturnResult(_sport, xia_socket_msg, rc, ec);
}
#endif


void XTRANSPORT::Xclose(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	int control_port = _sport;
	int ref;
	bool should_delete;

	xia::X_Close_Msg *xcm = xia_socket_msg->mutable_x_close();
	_sport = xcm->port();

	sock *sk = portToSock.get(_sport);
	bool teardown_now = true;

	if (!sk) {
		// this shouldn't happen!
		ERROR("Invalid socket %d\n", _sport);
		ReturnResult(control_port, xia_socket_msg, -1, EBADF);
		return;
	}

	assert(sk->refcount != 0);

	should_delete = sk->isAcceptedSocket;
	ref = --sk->refcount;

	if (ref == 0) {

		INFO("closing %d state=%s new refcount = %d\n", sk->port, StateStr(sk->state), ref);

		switch (sk -> get_type()) {
			case SOCK_STREAM:
				teardown_now = false;
				dynamic_cast<XStream *>(sk)->usrclosed();
				break;
			case SOCK_DGRAM:
				break;
			case SOCK_CHUNK:
				break;
		}

		if (teardown_now) {
			TeardownSocket(sk);
		}

	} else {
		// the app was forked and not everyone has closed the socket yet
		INFO("decremented ref count on %d state=%s new refcount=%d\n", sk->port, StateStr(sk->state), ref);
	}

	xcm->set_refcount(ref);
	xcm->set_delkeys(should_delete);
	ReturnResult(control_port, xia_socket_msg);
}



void XTRANSPORT::Xconnect(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Connect_Msg *x_connect_msg = xia_socket_msg->mutable_x_connect();
	String dest(x_connect_msg->ddag().c_str());
	XIAPath dst_path;
	sock *sk = portToSock.get(_sport);

	std::cout << "Reached Xconnect with Dest = " << dest << "\n";

	dst_path.parse(dest);

	if (!sk) {
		// FIXME: WHY WOULD THIS HAPPEN?
		ERROR("Invalid socket %d\n", _sport);
		sk = new sock();

	}
	// else if (sk->state != INACTIVE) {
	// 	// a connect is already in progress
	// 	x_connect_msg->set_status(xia::X_Connect_Msg::XCONNECTING);
	// 	sk->so_error = EALREADY;
	// 	ReturnResult(_sport, xia_socket_msg, -1, EALREADY);
	// }
	if (sk ->get_type() == SOCK_STREAM) {
		XStream *tcp_conn = dynamic_cast<XStream *>(sk);
		if (tcp_conn -> tp->t_state == TCPS_SYN_SENT) {
			// a connect is already in progress
			x_connect_msg->set_status(X_Connect_Msg::XCONNECTING);
			ReturnResult(_sport, xia_socket_msg, -1, EALREADY);
		}

		tcp_conn->dst_path = dst_path;
		tcp_conn->port = _sport;
		ChangeState(tcp_conn, SYN_SENT);
		tcp_conn->num_connect_tries++;

		String str_local_addr = _local_addr.unparse_re();

		// API sends a temporary DAG, if permanent not assigned by bind
		if (x_connect_msg->has_sdag()) {
			String sdag_string(x_connect_msg->sdag().c_str(), x_connect_msg->sdag().size());
			tcp_conn->src_path.parse(sdag_string);
		}

		// FIXME: is it possible for us not to have a source dag
		//   and if so, we should return an error
		assert(tcp_conn->src_path.is_valid());
		tcp_conn->set_nxt(LAST_NODE_DEFAULT);
		tcp_conn->set_last(LAST_NODE_DEFAULT);

		XID source_xid = tcp_conn->src_path.xid(tcp_conn->src_path.destination_node());
		XID destination_xid = tcp_conn->dst_path.xid(tcp_conn->dst_path.destination_node());

		XIDpair xid_pair;
		xid_pair.set_src(source_xid);
		xid_pair.set_dst(destination_xid);

		// Map the src & dst XID pair to source port()
		XIDpairToSock.set(xid_pair, tcp_conn);

		// Map the source XID to source port
		XIDtoSock.set(source_xid, tcp_conn);

		// Make us routable
		addRoute(source_xid);
		tcp_conn->usropen();
		ChangeState(tcp_conn, SYN_SENT);
		//std::cout << "SYN-SENT\n";
		ChangeState(sk, SYN_SENT);

		// We return EINPROGRESS no matter what. If we're in non-blocking mode, the
		// API will pass EINPROGRESS on to the app. If we're in blocking mode, the API
		// will wait until it gets another message from xtransport notifying it that
		// the other end responded and the connection has been CONNECTED.
		x_connect_msg->set_status(xia::X_Connect_Msg::XCONNECTING);
		tcp_conn->so_error = EINPROGRESS;
		ReturnResult(_sport, xia_socket_msg, -1, EINPROGRESS);
	}
	// // Prepare SYN packet
	// const char *payload = "SYN";
	// SendControlPacket(TransportHeader::SYN, sk, payload, strlen(payload), dst_path, sk->src_path);
}



void XTRANSPORT::Xlisten(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// we just want to mark the socket as listenening and return right away.

	// FIXME: we should make sure we are already bound to a DAG
	// FIXME: make sure no one else is bound to this DAG

	xia::X_Listen_Msg *x_listen_msg = xia_socket_msg->mutable_x_listen();

	INFO("Socket %d Xlisten\n", _sport);

	sock *sk = portToSock.get(_sport);
	std::cout << _sport << " xcache = " << sk->xcacheSock << " Listen\n";
	if (sk->state == INACTIVE || sk->state == LISTEN) {
		ChangeState(sk, LISTEN);
		sk->backlog = x_listen_msg->backlog();
	} else {
		// FIXME: what is the correct error code to return
	}

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::XreadyToAccept(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	sock *sk = portToSock.get(_sport);

	if (!sk->pending_connection_buf.empty()) {
		// If there is already a pending connection, return true now
		INFO("Pending connection is not empty\n");

		ReturnResult(_sport, xia_socket_msg);


	} else if (xia_socket_msg->blocking()) {
		// If not and we are blocking, add this request to the pendingAccept queue and wait

		INFO("Pending connection is empty\n");

		// xia_socket_msg is on the stack; allocate a copy on the heap
		xia::XSocketMsg *xsm_cpy = new xia::XSocketMsg();
		xsm_cpy->CopyFrom(*xia_socket_msg);
		sk->pendingAccepts.push(xsm_cpy);

	} else {
		// socket is non-blocking and nothing is ready yet
		ReturnResult(_sport, xia_socket_msg, -1, EWOULDBLOCK);
	}
}



void XTRANSPORT::Xaccept(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	int rc = 0, ec = 0;

	// _sport is the *existing accept socket*
	unsigned short new_port = xia_socket_msg->x_accept().new_port();
	sock *sk = portToSock.get(_sport);

	DBG("_sport %d, new_port %d seq:%d\n", _sport, new_port, xia_socket_msg->sequence());
	DBG("p buf size = %d\n", sk->pending_connection_buf.size());
	DBG("blocking = %d\n", sk->isBlocking);

	sk->hlim = HLIM_DEFAULT;
	sk->nxt_xport = CLICK_XIA_NXT_XTCP;

	if (!sk->pending_connection_buf.empty()) {
		sock *new_sk = sk->pending_connection_buf.front();

		DBG("Get front element from and assign port number %d.", new_port);

		if (new_sk->state != CONNECTED) {
			ERROR("ERROR: sock from pending_connection_buf !isconnected\n");
		} else {
			INFO("Socket on port %d is now connected\n", new_port);
		}
		new_sk->port = new_port;
		ChangeState(new_sk, CONNECTED);
		new_sk->isAcceptedSocket = true;

		portToSock.set(new_port, new_sk);

		sk->pending_connection_buf.pop();



// FIXME: does this block of code do anything??? I don't see the payload getting used
// I think it's all happening in the syn handling above?
/*
		WritablePacket *just_payload_part;
		int payloadLength;
		std::cout << "Xcachesock = " <<sk->xcacheSock << "\n";
		if((sk->xcacheSock == false) && usingRendezvousDAG(sk->src_path, new_sk->src_path)) {
			XID _destination_xid = new_sk->src_path.xid(new_sk->src_path.destination_node());
			INFO("Xaccept: Sending SYNACK with verification for RV DAG");
			// Destination DAG from the SYN packet
			String src_path_str = new_sk->src_path.unparse();

			// Current timestamp as nonce against replay attacks
			Timestamp now = Timestamp::now();
			double timestamp = strtod(now.unparse().c_str(), NULL);

			// Build the payload with DAG for this service and timestamp
			XIASecurityBuffer synackPayload(1024);
			synackPayload.pack(src_path_str.c_str(), src_path_str.length());
			synackPayload.pack((const char *)&timestamp, (uint16_t) sizeof timestamp);

			// Sign the synack payload
			char signature[MAX_SIGNATURE_SIZE];
			uint16_t signatureLength = MAX_SIGNATURE_SIZE;
			if (xs_sign(_destination_xid.unparse().c_str(), (unsigned char *)synackPayload.get_buffer(), synackPayload.size(), (unsigned char *)signature, &signatureLength)) {
				ERROR("ERROR unable to sign the SYNACK using private key for %s", _destination_xid.unparse().c_str());
				rc = -1;
				ec = ESTALE;
				goto Xaccept_done;
			}

			// Retrieve public key for this host
			char pubkey[MAX_PUBKEY_SIZE];
			uint16_t pubkeyLength = MAX_PUBKEY_SIZE;
			if (xs_getPubkey(_destination_xid.unparse().c_str(), pubkey, &pubkeyLength)) {
				ERROR("ERROR public key not found for %s", _destination_xid.unparse().c_str());
				rc = -1;
				ec = ESTALE;
				goto Xaccept_done;
			}

			// Prepare a signed payload (serviceDAG, timestamp)Signature, Pubkey
			XIASecurityBuffer signedPayload(2048);
			signedPayload.pack(synackPayload.get_buffer(), synackPayload.size());
			signedPayload.pack(signature, signatureLength);
			signedPayload.pack((char *)pubkey, pubkeyLength);

			just_payload_part = WritablePacket::make(256, (const void*)signedPayload.get_buffer(), signedPayload.size(), 1);
			payloadLength = signedPayload.size();
		}

		else {
			const char* dummy = "Connection_granted";
			just_payload_part = WritablePacket::make(256, dummy, strlen(dummy), 0);
			payloadLength = strlen(dummy);
		}
		*/

		// Get remote DAG to return to app
		xia::X_Accept_Msg *x_accept_msg = xia_socket_msg->mutable_x_accept();
		x_accept_msg->set_remote_dag(new_sk->dst_path.unparse().c_str()); // remote endpoint is dest from our perspective
		if(xia_socket_msg->x_accept().has_sendmypath()) {
			std::cout << "Flag sendremotepath set " << new_sk->src_path.unparse().c_str() << "\n";
			x_accept_msg->set_self_dag(new_sk->src_path.unparse().c_str());
		}
	} else {
		// FIXME: THIS BETTER NOT HAPPEN!
		INFO("Got EWOULDBLOCK on a blocking accept!");
		rc = -1;
		ec = EWOULDBLOCK;
		goto Xaccept_done;
	}

Xaccept_done:
	ReturnResult(_sport, xia_socket_msg, rc, ec);
}



void XTRANSPORT::Xupdaterv(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	UNUSED(_sport);
	UNUSED(xia_socket_msg);
#if 0
	sock *sk = portToSock.get(_sport);

	// Retrieve rendezvous service DAG from user provided argument
	xia::X_Updaterv_Msg *x_updaterv_msg = xia_socket_msg->mutable_x_updaterv();
	String rendezvousDAGstr(x_updaterv_msg->rvdag().c_str());
	XIAPath rendezvousDAG;
	rendezvousDAG.parse(rendezvousDAGstr, NULL);

	// HID of this host
	String myHID = _local_hid.unparse();

	// Current local DAG for this host
	XIAPath localDAG = local_addr();
	String localDAGstr = localDAG.unparse();

	// Current timestamp as nonce against replay attacks
	Timestamp now = Timestamp::now();
	double timestamp = strtod(now.unparse().c_str(), NULL);

	// Message going out to the rendezvous server
	INFO("RV DAG:%s", rendezvousDAGstr.c_str());
	INFO("for:%s", myHID.c_str());
	INFO("at:%s", localDAGstr.c_str());
	INFO("timestamp: %f", timestamp);

	// Prepare control message for rendezvous service
	XIASecurityBuffer controlMsg = XIASecurityBuffer(1024);
	controlMsg.pack(myHID.c_str(), myHID.length());
	controlMsg.pack(localDAGstr.c_str(), localDAGstr.length());
	controlMsg.pack((const char *)&timestamp, (uint16_t) sizeof timestamp);

	// Sign the control message
	char signature[MAX_SIGNATURE_SIZE];
	uint16_t signatureLength = MAX_SIGNATURE_SIZE;
	xs_sign(myHID.c_str(), (unsigned char *)controlMsg.get_buffer(), controlMsg.size(), (unsigned char *)signature, &signatureLength);

	// Retrieve public key for this host
	char pubkey[MAX_PUBKEY_SIZE];
	uint16_t pubkeyLength = MAX_PUBKEY_SIZE;
	if (xs_getPubkey(myHID.c_str(), pubkey, &pubkeyLength)) {
		ERROR("ERROR public key not found for %s", myHID.c_str());
		return;
	}

	// Build signed message (hid, newDAG, timestamp)Signature, Pubkey
	XIASecurityBuffer signedMsg = XIASecurityBuffer(2048);
	signedMsg.pack(controlMsg.get_buffer(), controlMsg.size());
	signedMsg.pack(signature, signatureLength);
	signedMsg.pack((char *)pubkey, pubkeyLength);

	// Prepare XIP header
	XIAHeaderEncap xiah;
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(sk->hlim);
	xiah.set_dst_path(rendezvousDAG);
	xiah.set_src_path(sk->src_path);

	WritablePacket *just_payload_part = WritablePacket::make(256, (const void*)signedMsg.get_buffer(), signedMsg.size(), 1);

	WritablePacket *p = NULL;

	xiah.set_nxt(CLICK_XIA_NXT_TRN);
	xiah.set_plen(signedMsg.size());

	//Add XIA Transport headers
	TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDGRAMHeader(0); // length
	p = thdr->encap(just_payload_part);

	thdr->update();
	xiah.set_plen(signedMsg.size() + thdr->hlen()); // XIA payload = transport header + transport-layer data

	p = xiah.encap(p, false);
	delete thdr;

	output(NETWORK_PORT).push(p);
#endif
}



// note this is only going to return status for a single socket in the poll response
// the only time we will return multiple sockets is when poll returns immediately
// TODO: is it worth changing this to possibly return more than one event?
void XTRANSPORT::ProcessPollEvent(unsigned short _sport, unsigned int flags_out)
{
	// loop thru all the polls that are registered looking for the socket associated with _sport
	for (HashTable<unsigned short, PollEvent>::iterator it = poll_events.begin(); it != poll_events.end(); it++) {
		unsigned short pollport = it->first;
		PollEvent pe = it->second;

		HashTable<unsigned short, unsigned int>::iterator sevent = pe.events.find(_sport);

		// socket isn't in this poll instance, keep looking
		if (sevent == pe.events.end())
			continue;

		unsigned short port = sevent->first;
		unsigned int mask = sevent->second;

		// if flags_out isn't an error and doesn't match the event mask keep looking
		if (!(flags_out & mask) && !(flags_out & (POLLHUP | POLLERR | POLLNVAL)))
			continue;

		xia::XSocketMsg xsm;
		xsm.set_type(xia::XPOLL);
		xia::X_Poll_Msg *msg = xsm.mutable_x_poll();

		msg->set_nfds(1);
		msg->set_type(xia::X_Poll_Msg::RESULT);

		xia::X_Poll_Msg::PollFD *pfd = msg->add_pfds();
		pfd->set_flags(flags_out);
		pfd->set_port(port);

		// do I need to set other flags in the return struct?
		ReturnResult(pollport, &xsm, 1, 0);

		// found the socket, decrement the polling count for all the sockets in the poll instance
		for (HashTable<unsigned short, unsigned int>::iterator pit = pe.events.begin(); pit != pe.events.end(); pit++) {
			port = pit->first;

			sock *sk = portToSock.get(port);
			sk->polling--;
		}

		// get rid of this poll event
		poll_events.erase(it);
	}
}



void XTRANSPORT::CancelPollEvent(unsigned short _sport)
{
	PollEvent pe;
	unsigned short pollport;
	HashTable<unsigned short, PollEvent>::iterator it;

	// loop thru all the polls that are registered looking for the socket associated with _sport
	for (it = poll_events.begin(); it != poll_events.end(); it++) {
		pollport = it->first;
		pe = it->second;

		if (pollport == _sport)
			break;
		pollport = 0;
	}

	if (pollport == 0) {
		// we didn't find any events for this control socket
		// should we report error in this case?
		return;
	}

	// we have the poll event associated with this control socket

	// decrement the polling count for all the sockets in the poll instance
	for (HashTable<unsigned short, unsigned int>::iterator pit = pe.events.begin(); pit != pe.events.end(); pit++) {
		unsigned short port = pit->first;

		sock *sk = portToSock.get(port);
		if (sk) {
			sk->polling--;
		}
	}

	// get rid of this poll event
	poll_events.erase(it);
}



void XTRANSPORT::CreatePollEvent(unsigned short _sport, xia::X_Poll_Msg *msg)
{
	PollEvent pe;
	uint32_t nfds = msg->nfds();

	for (unsigned i = 0; i < nfds; i++) {
		const xia::X_Poll_Msg::PollFD& pfd = msg->pfds(i);

		int port = pfd.port();
		unsigned flags = pfd.flags();

		// ignore ports that are set to 0, or are negative
		if (port <= 0)
			continue;

		// add the socket to this poll event
		pe.events.set(port, flags);
		sock *sk = portToSock.get(port);

		// let the socket know a poll is enabled on it
		sk->polling++;
	}

	// register the poll event
	poll_events.set(_sport, pe);
}



void XTRANSPORT::Xpoll(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Poll_Msg *poll_in = xia_socket_msg->mutable_x_poll();

	if (poll_in->type() == xia::X_Poll_Msg::DOPOLL) {

		int actionable = 0;
		xia::XSocketMsg msg_out;
		msg_out.set_type(xia::XPOLL);
		xia::X_Poll_Msg *poll_out = msg_out.mutable_x_poll();

		unsigned nfds = poll_in->nfds();

		for (unsigned i = 0; i < nfds; i++) {
			const xia::X_Poll_Msg::PollFD& pfd_in = poll_in->pfds(i);

			int port = pfd_in.port();
			unsigned flags = pfd_in.flags();

			// skip over ignored ports
			if ( port <= 0) {
				continue;
			}

			sock *sk = portToSock.get(port);
			unsigned flags_out = 0;

			if (!sk) {
				// no socket state, we'll return an error right away
				flags_out = POLLNVAL;

			} else {
				// is there any read data?
				if (flags & POLLIN) {
					if (sk->sock_type == SOCK_STREAM) {
						if (sk->recv_base < sk->next_recv_seqnum) {
							flags_out |= POLLIN;
						} else if (sk->state == CLOSE_WAIT) {
							// other end closed, app needs to know!
							flags_out |= POLLIN;
						}

// MERGE need to get this code back in for non-block connects
//						if (!sk->pending_connection_buf.empty()) {
//							INFO("%d accepts are pending\n", sk->pending_connection_buf.size());
//							flags_out |= POLLIN | POLLOUT;
//						}
//
					} else if (sk->sock_type == SOCK_DGRAM || sk->sock_type == SOCK_RAW) {
						if (sk->recv_buffer_count > 0) {
							flags_out |= POLLIN;
						}
					}
				}

				if (flags & POLLOUT) {
					// see if the socket is writable
					// FIXME should we be looking for anything else (send window, etc...)
					if (sk->sock_type == SOCK_STREAM) {
						if (sk->state == CONNECTED) {
							flags_out |= POLLOUT;
						}

					} else {
						flags_out |= POLLOUT;
					}
				}
			}

			if (flags_out) {
				// the socket can respond to the poll immediately
				xia::X_Poll_Msg::PollFD *pfd_out = poll_out->add_pfds();
				pfd_out->set_flags(flags_out);
				pfd_out->set_port(port);

				actionable++;
			}
		}

		// we can return a result right away
		if (actionable) {
			poll_out->set_nfds(actionable);
			poll_out->set_type(xia::X_Poll_Msg::RESULT);

			ReturnResult(_sport, &msg_out, actionable, 0);

		} else {
			// we can't return a result yet
			CreatePollEvent(_sport, poll_in);
		}
	} else { // type == CANCEL
		// cancel the poll(s) on this control socket
		CancelPollEvent(_sport);
	}
}



void XTRANSPORT::Xchangead(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// Save the old AD
	String str_local_addr = _local_addr.unparse();
	size_t old_AD_start = str_local_addr.find_left("AD:");
	size_t old_AD_end = str_local_addr.find_left(" ", old_AD_start);
	String old_AD_str = str_local_addr.substring(old_AD_start, old_AD_end - old_AD_start);

	xia::X_Changead_Msg *x_changead_msg = xia_socket_msg->mutable_x_changead();
	//String tmp = _local_addr.unparse();
	//Vector<String> ids;
	//cp_spacevec(tmp, ids);
	String AD_str(x_changead_msg->ad().c_str());
	String HID_str = _local_hid.unparse();
	String IP4ID_str(x_changead_msg->ip4id().c_str());
	_local_4id.parse(IP4ID_str);
	String new_local_addr;
	// If a valid 4ID is given, it is included (as a fallback) in the local_addr
	if (_local_4id != _null_4id) {
		new_local_addr = "RE ( " + IP4ID_str + " ) " + AD_str + " " + HID_str;
	} else {
		new_local_addr = "RE " + AD_str + " " + HID_str;
	}
	NOTICE("new address is - %s", new_local_addr.c_str());
	_local_addr.parse(new_local_addr);
#if 0
	// Inform all active stream connections about this change
	for (HashTable<unsigned short, sock*>::iterator iter = portToSock.begin(); iter != portToSock.end(); ++iter ) {
		unsigned short _migrateport = iter->first;
		sock *sk = portToSock.get(_migrateport);
		// Skip non-stream connections
		if (sk->sock_type != SOCK_STREAM) {
			continue;
		}
		// Skip inactive ports
		if (sk->state != CONNECTED) {
			INFO("skipping migration for non-connected port");
			INFO("src_path:%s:", sk->src_path.unparse().c_str());
			continue;
		}
		// Update src_path in sk
		INFO("updating %s to %s in sk", old_AD_str.c_str(), AD_str.c_str());
		sk->src_path.replace_node_xid(old_AD_str, AD_str);

		// Send MIGRATE message to each corresponding endpoint
		// src_DAG, dst_DAG, timestamp - Signed by private key
		// plus the public key (Should really exchange at SYN/SYNACK)
		uint8_t *payload;
		uint8_t *payloadptr;
		uint32_t maxpayloadlen;
		uint32_t payloadlen;
		String src_path = sk->src_path.unparse();
		String dst_path = sk->dst_path.unparse();
		INFO("MIGRATING %s - %s", src_path.c_str(), dst_path.c_str());
		int src_path_len = strlen(src_path.c_str()) + 1;
		int dst_path_len = strlen(dst_path.c_str()) + 1;
		Timestamp now = Timestamp::now();
		String timestamp = now.unparse();
		int timestamp_len = strlen(timestamp.c_str()) + 1;
		// Get the public key to include in packet
		char pubkey[MAX_PUBKEY_SIZE];
		uint16_t pubkeylen = MAX_PUBKEY_SIZE;
		XID src_xid = sk->src_path.xid(sk->src_path.destination_node());
		INFO("Retrieving pubkey for xid:%s:", src_xid.unparse().c_str());
		if (xs_getPubkey(src_xid.unparse().c_str(), pubkey, &pubkeylen)) {
			INFO("ERROR: Pubkey not found:%s:", src_xid.unparse().c_str());
			return;
		}
		INFO("Pubkey:%s:", pubkey);
		maxpayloadlen = src_path_len + dst_path_len + timestamp_len + sizeof(uint16_t) + MAX_SIGNATURE_SIZE + sizeof(uint16_t) + pubkeylen;
		payload = (uint8_t *)calloc(maxpayloadlen, 1);
		if (payload == NULL) {
			ERROR("ERROR: Cannot allocate memory for Migrate packet");
			return;
		}
		// Build the payload
		payloadptr = payload;
		// Source DAG with new AD
		memcpy(payloadptr, src_path.c_str(), src_path_len);
		INFO("MIGRATE Source DAG: %s", payloadptr);
		payloadptr += src_path_len;
		// Destination DAG
		memcpy(payloadptr, dst_path.c_str(), dst_path_len);
		INFO("MIGRATE Dest DAG: %s", payloadptr);
		payloadptr += dst_path_len;
		// Timestamp of this MIGRATE message
		memcpy(payloadptr, timestamp.c_str(), timestamp_len);
		INFO("MIGRATE Timestamp: %s", timestamp.c_str());
		payloadptr += timestamp_len;
		// Sign(SourceDAG, DestinationDAG, Timestamp)
		uint8_t signature[MAX_SIGNATURE_SIZE];
		uint16_t siglen = MAX_SIGNATURE_SIZE;
		if (xs_sign(src_xid.unparse().c_str(), payload, payloadptr - payload, signature, &siglen)) {
			ERROR("ERROR: Signing Migrate packet");
			return;
		}
		// Signature length
		memcpy(payloadptr, &siglen, sizeof(uint16_t));
		payloadptr += sizeof(uint16_t);
		// Signature
		memcpy(payloadptr, signature, siglen);
		payloadptr += siglen;
		// Public key length
		memcpy(payloadptr, &pubkeylen, sizeof(uint16_t));
		payloadptr += sizeof(uint16_t);
		// Public key of source SID
		memcpy(payloadptr, pubkey, pubkeylen);
		INFO("MIGRATE: Pubkey:%s: Length: %d", pubkey, pubkeylen);
		payloadptr += pubkeylen;
		// Total payload length
		payloadlen = payloadptr - payload;
		INFO("MIGRATE payload length: %d", payloadlen);

		//Add XIA headers
		XIAHeaderEncap xiah;
		xiah.set_nxt(CLICK_XIA_NXT_TRN);
		xiah.set_last(LAST_NODE_DEFAULT);
		xiah.set_hlim(sk->hlim);
		xiah.set_dst_path(sk->dst_path);
		xiah.set_src_path(sk->src_path);

		WritablePacket *just_payload_part = WritablePacket::make(256, payload, payloadlen, 0);
		free(payload);

		WritablePacket *p = NULL;

		TransportHeaderEncap *thdr = TransportHeaderEncap::MakeMIGRATEHeader( 0, 0, 0, calc_recv_window(sk)); // #seq, #ack, length

		p = thdr->encap(just_payload_part);

		thdr->update();
		xiah.set_plen(payloadlen + thdr->hlen()); // XIA payload = transport header + transport-layer data

		p = xiah.encap(p, false);

		delete thdr;

		// Store the migrate packet for potential retransmission
		sk->migrate_pkt = copy_packet(p, sk);
		sk->num_migrate_tries++;
		sk->last_migrate_ts = timestamp;

		// Set timer
		sk->migrateack_waiting = true;
		ScheduleTimer(sk, MIGRATEACK_DELAY);

		portToSock.set(_migrateport, sk);
		if (_migrateport != sk->port) {
			ERROR("ERROR _sport %d, sk->port %d", _migrateport, sk->port);
		}
		output(NETWORK_PORT).push(p);
	}
#endif
	ReturnResult(_sport, xia_socket_msg);


	// now also tell anyone who is waiting on a notification this happened
	list<int>::iterator i;
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XNOTIFY);
	xsm.set_sequence(0);

	for (i = notify_listeners.begin(); i != notify_listeners.end(); i++) {
		ReturnResult(*i, &xsm);
	}
	// get rid of them all now so we can start fresh
	notify_listeners.clear();
}



void XTRANSPORT::Xreadlocalhostaddr(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// read the localhost AD and HID
	String local_addr = _local_addr.unparse();
	size_t AD_found_start = local_addr.find_left("AD:");
	size_t AD_found_end = local_addr.find_left(" ", AD_found_start);
	String AD_str = local_addr.substring(AD_found_start, AD_found_end - AD_found_start);
	String HID_str = _local_hid.unparse();
	String IP4ID_str = _local_4id.unparse();
	// return a packet containing localhost AD and HID
	xia::X_ReadLocalHostAddr_Msg *_msg = xia_socket_msg->mutable_x_readlocalhostaddr();
	_msg->set_ad(AD_str.c_str());
	_msg->set_hid(HID_str.c_str());
	_msg->set_ip4id(IP4ID_str.c_str());

	ReturnResult(_sport, xia_socket_msg);
}


void XTRANSPORT::XsetXcacheSid(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	UNUSED(_sport);
	xia::X_SetXcacheSid_Msg *_msg = xia_socket_msg->mutable_x_setxcachesid();

	_xcache_sid.parse(_msg->sid().c_str());
}


void XTRANSPORT::Xupdatenameserverdag(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Updatenameserverdag_Msg *x_updatenameserverdag_msg = xia_socket_msg->mutable_x_updatenameserverdag();
	String ns_dag(x_updatenameserverdag_msg->dag().c_str());
	_nameserver_addr.parse(ns_dag);

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xreadnameserverdag(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// read the nameserver DAG
	String ns_addr = _nameserver_addr.unparse();

	// return a packet containing the nameserver DAG
	xia::X_ReadNameServerDag_Msg *_msg = xia_socket_msg->mutable_x_readnameserverdag();
	_msg->set_dag(ns_addr.c_str());

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xisdualstackrouter(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// return a packet indicating whether this node is an XIA-IPv4 dual-stack router
	xia::X_IsDualStackRouter_Msg *_msg = xia_socket_msg->mutable_x_isdualstackrouter();
	_msg->set_flag(_is_dual_stack_router);

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xgetpeername(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	sock *sk = portToSock.get(_sport);

	xia::X_GetPeername_Msg *_msg = xia_socket_msg->mutable_x_getpeername();
	_msg->set_dag(sk->dst_path.unparse().c_str());

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xgetsockname(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	sock *sk = portToSock.get(_sport);

	xia::X_GetSockname_Msg *_msg = xia_socket_msg->mutable_x_getsockname();
	_msg->set_dag(sk->src_path.unparse().c_str());

	ReturnResult(_sport, xia_socket_msg);
}



void XTRANSPORT::Xsend(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in)
{
	int rc = 0, ec = 0;

	//Find socket state
	sock *sk = portToSock.get(_sport);

	// Make sure the socket state isn't null
	if (rc == 0 && !sk) {
		rc = -1;
		ec = EBADF; // FIXME: is this the right error?
	}

	xia::X_Send_Msg *x_send_msg = xia_socket_msg->mutable_x_send();
	int pktPayloadSize = x_send_msg->payload().size();

	//Find DAG info for that stream
	if (rc == 0 && sk->sock_type == SOCK_RAW) {
		char payload[65536];
		memcpy(payload, x_send_msg->payload().c_str(), pktPayloadSize);

		// FIXME: we should check to see that the packet isn't too big here
		//
		struct click_xia *xiah = reinterpret_cast<struct click_xia *>(payload);

		XIAHeader xiaheader(xiah);
		XIAHeaderEncap xiahencap(xiaheader);
		XIAPath dst_path = xiaheader.dst_path();
		INFO("Sending RAW packet to:%s:", dst_path.unparse().c_str());
		size_t headerlen = xiaheader.hdr_size();
		char *pktcontents = &payload[headerlen];
		int pktcontentslen = pktPayloadSize - headerlen;
		INFO("Packet size without XIP header:%d", pktcontentslen);

		WritablePacket *p = WritablePacket::make(p_in->headroom() + 1, (const void*)pktcontents, pktcontentslen, p_in->tailroom());
		p = xiahencap.encap(p, false);

		output(NETWORK_PORT).push(p);
		x_send_msg->clear_payload(); // clear payload before returning result
		ReturnResult(_sport, xia_socket_msg);
		return;
	}

	// Make sure socket is connected
	if (rc == 0 && sk->state != CONNECTED) {
		ERROR("ERROR: Socket on port %d was not connected!\n", sk->port);
		rc = -1;
		ec = ENOTCONN;
	}


	// If everything is OK so far, try sending
	if (rc == 0) {
		rc = pktPayloadSize;

		//Recalculate source path
		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse();
		//Make source DAG _local_addr:SID
		String dagstr = sk->src_path.unparse_re();

		//Client Mobility...
		if (dagstr.length() != 0 && dagstr != str_local_addr) {
			//Moved!
			// 1. Update 'sk->src_path'
			sk->src_path.parse_re(str_local_addr);
		}

		// Case of initial binding to only SID
		if (sk->full_src_dag == false) {
			sk->full_src_dag = true;
			String str_local_addr = _local_addr.unparse_re();
			XID front_xid = sk->src_path.xid(sk->src_path.destination_node());
			String xid_string = front_xid.unparse();
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
			sk->src_path.parse_re(str_local_addr);
		}

		//Add XIA headers
		WritablePacket *payload = WritablePacket::make(p_in->headroom() + 1, (const void*)x_send_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

		if (sk -> get_type() == SOCK_STREAM) {	// why do we need this test, we should always be a stream socket here
			XStream *st = dynamic_cast<XStream *>(sk);

			int tcp_rc = st->usrsend(payload);
			//INFO("usrsend reurned %d\n", tcp_rc);
			if (tcp_rc != 0) {
				if (tcp_rc == EPIPE) {
					// the socket is closing
					ec = rc; // ? is this right???? if so should we signal from the API?/
					rc = -1;

				} else if (!xia_socket_msg->blocking()) {
					rc = -1;
					ec = EWOULDBLOCK;

				} else if (st->stage_data(payload, xia_socket_msg->sequence()) != false) {
					// transmit queue is full. Saving the data and not returning
					// until later when it has be added to the queue
					DBG("Transmit buffer full, saving and blocking\n");
					return;
				} else {
					// in theory this should never happen
					rc = -1;
					ec = ENOBUFS;
				}

			} else {
//				DBG("(%d) sent packet to %s, from %s\n", _sport, sk->dst_path.unparse_re().c_str(), sk->src_path.unparse_re().c_str());
			}

//			// FIXME: what is this for? we should already be in the table
//			portToSock.set(_sport, sk);
//			if (_sport != sk->port) {
//				ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
//			}
		}
	}

	x_send_msg->clear_payload(); // clear payload before returning result
	ReturnResult(_sport, xia_socket_msg, rc, ec);
}



void XTRANSPORT::Xsendto(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in)
{
	int rc = 0, ec = 0;

	xia::X_Sendto_Msg *x_sendto_msg = xia_socket_msg->mutable_x_sendto();

	String dest(x_sendto_msg->ddag().c_str());
	int pktPayloadSize = x_sendto_msg->payload().size();

	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	sock *sk = portToSock.get(_sport);

	if (!sk) {
		//No local SID bound yet, so bind one
		sk = new sock();
	}

	if (sk->initialized == false) {
		sk->initialized = true;
		sk->full_src_dag = true;
		if (sk->port != _sport) {
			INFO("sk->port was %d setting to %d", sk->port, _sport);
		}
		sk->port = _sport;
		String str_local_addr = _local_addr.unparse_re();

		char xid_string[50];
		random_xid("SID", xid_string);
		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID

		sk->src_path.parse_re(str_local_addr);

		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());

		XIDtoSock.set(source_xid, sk); //Maybe change the mapping to XID->sock?
		addRoute(source_xid);
	}

	// Case of initial binding to only SID
	if (sk->full_src_dag == false) {
		sk->full_src_dag = true;
		String str_local_addr = _local_addr.unparse_re();
		XID front_xid = sk->src_path.xid(sk->src_path.destination_node());
		String xid_string = front_xid.unparse();
		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
		sk->src_path.parse_re(str_local_addr);
	}


	if (sk->src_path.unparse_re().length() != 0) {
		//Recalculate source path
		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
		sk->src_path.parse(str_local_addr);
	}

	portToSock.set(_sport, sk);
	if (_sport != sk->port) {
		ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
	}

	sk = portToSock.get(_sport);

	//Add XIA headers
	XIAHeaderEncap xiah;

	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(sk->hlim);
	xiah.set_dst_path(dst_path);
	xiah.set_src_path(sk->src_path);

	WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_sendto_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

	WritablePacket *p = NULL;

	if (sk->sock_type == SOCK_RAW) {
		xiah.set_nxt(sk->nxt_xport);

		xiah.set_plen(pktPayloadSize);
		p = xiah.encap(just_payload_part, false);

	} else {
		xiah.set_nxt(CLICK_XIA_NXT_TRN);
		xiah.set_plen(pktPayloadSize);

		//Add XIA Transport headers
		TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDGRAMHeader(0); // length
		p = thdr->encap(just_payload_part);

		thdr->update();
		xiah.set_plen(pktPayloadSize + thdr->hlen()); // XIA payload = transport header + transport-layer data

		p = xiah.encap(p, false);
		delete thdr;
	}

	output(NETWORK_PORT).push(p);

	rc = pktPayloadSize;
	x_sendto_msg->clear_payload();
	ReturnResult(_sport, xia_socket_msg, rc, ec);
}



void XTRANSPORT::Xrecv(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	sock *sk = portToSock.get(_sport);

	if (sk->port != _sport) {
		ERROR("ERROR sk->port %d _sport %d", sk->port, _sport);
		// FIXME: do something with the error
	}

	if (sk && (sk->state == CONNECTED || sk->state == CLOSE_WAIT)) {

		if (sk -> sock_type == SOCK_STREAM)
		{
			((XStream *)sk) -> read_from_recv_buf(xia_socket_msg);
		} else if (sk -> sock_type == SOCK_DGRAM)
		{
			((XStream *)sk) -> read_from_recv_buf(xia_socket_msg);
		}

		if (xia_socket_msg->x_recv().bytes_returned() > 0) {
			// Return response to API
			INFO("Read %d bytes", xia_socket_msg->x_recv().bytes_returned());
			ReturnResult(_sport, xia_socket_msg, xia_socket_msg->x_recv().bytes_returned());

		} else if (sk->state == CLOSE_WAIT) {
			// other end has closed, tell app there's nothing to read
			// what if other end is doing retransmits??
			ReturnResult(_sport, xia_socket_msg, 0);

		} else if (!xia_socket_msg->blocking()) {

			// we're not blocking and there's no data, so let API know immediately
			sk->recv_pending = false;
			ReturnResult(_sport, xia_socket_msg, -1, EWOULDBLOCK);

		} else {
			// rather than returning a response, wait until we get data
			sk->recv_pending = true; // when we get data next, send straight to app

			// xia_socket_msg is saved on the stack; allocate a copy on the heap
			xia::XSocketMsg *xsm_cpy = new xia::XSocketMsg();
			xsm_cpy->CopyFrom(*xia_socket_msg);
			sk->pending_recv_msg = xsm_cpy;
		}
	}
}


// FIXME: this is identical to Xrecv except for the protobuf type
// perhaps they should be combined
void XTRANSPORT::Xrecvfrom(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	// cout << "XRECVFROM IS CALLED" <<endl;

	// FIXME: do we even need this check???
	sock *sk = portToSock.get(_sport);
	if (sk->port != _sport) {
		ERROR("ERROR sk->port %d _sport %d", sk->port, _sport);
		// FIXME: do something with the error
	}

	dynamic_cast<XDatagram *>(sk) -> read_from_recv_buf(xia_socket_msg);

	if (xia_socket_msg->x_recvfrom().bytes_returned() > 0) {
		ReturnResult(_sport, xia_socket_msg, xia_socket_msg->x_recvfrom().bytes_returned());

	} else if (!xia_socket_msg->blocking()) {

		// we're not blocking and there's no data, so let API know immediately
		sk->recv_pending = false;
		ReturnResult(_sport, xia_socket_msg, -1, EWOULDBLOCK);

	} else {
		// rather than returning a response, wait until we get data
		sk->recv_pending = true; // when we get data next, send straight to app

		// xia_socket_msg is saved on the stack; allocate a copy on the heap
		xia::XSocketMsg *xsm_cpy = new xia::XSocketMsg();
		xsm_cpy->CopyFrom(*xia_socket_msg);
		sk->pending_recv_msg = xsm_cpy;
	}
}


#if 0
void XTRANSPORT::XrequestChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket *p_in)
{
	xia::X_Requestchunk_Msg *x_requestchunk_msg = xia_socket_msg->mutable_x_requestchunk();

	String pktPayload(x_requestchunk_msg->payload().c_str(), x_requestchunk_msg->payload().size());
	int pktPayloadSize = pktPayload.length();

	// send CID-Requests

	for (int i = 0; i < x_requestchunk_msg->dag_size(); i++) {
		String dest = x_requestchunk_msg->dag(i).c_str();
		XIAPath dst_path;
		dst_path.parse(dest);

		//Find DAG info for this DGRAM
		sock *sk = portToSock.get(_sport);

		if (!sk) {
			//No local SID bound yet, so bind one
			sk = new sock();
		}

		if (sk->initialized == false) {
			sk->initialized = true;
			sk->full_src_dag = true;
			sk->port = _sport;
			String str_local_addr = _local_addr.unparse_re();

			char xid_string[50];
			random_xid("SID", xid_string);
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID

			sk->src_path.parse_re(str_local_addr);

			XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());

			XIDtoSock.set(source_xid, sk);
			addRoute(source_xid);

		}

		// Case of initial binding to only SID
		if (sk->full_src_dag == false) {
			sk->full_src_dag = true;
			String str_local_addr = _local_addr.unparse_re();
			XID front_xid = sk->src_path.xid(sk->src_path.destination_node());
			String xid_string = front_xid.unparse();
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
			sk->src_path.parse_re(str_local_addr);
		}

		if (sk->src_path.unparse_re().length() != 0) {
			//Recalculate source path
			XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
			String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
			sk->src_path.parse(str_local_addr);
		}

		portToSock.set(_sport, sk);
		if (_sport != sk->port) {
			ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
		}

		sk = portToSock.get(_sport);

		//_errh->debug("sent packet to %s, from %s\n", dest.c_str(), sk->src_path.unparse_re().c_str());

		//Add XIA headers
		XIAHeaderEncap xiah;
		xiah.set_nxt(CLICK_XIA_NXT_CID);
		xiah.set_last(LAST_NODE_DEFAULT);
		xiah.set_hlim(sk->hlim);
		xiah.set_dst_path(dst_path);
		xiah.set_src_path(sk->src_path);
		xiah.set_plen(pktPayloadSize);

		WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_requestchunk_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

		WritablePacket *p = NULL;

		//Add Content header
		ContentHeaderEncap *chdr = ContentHeaderEncap::MakeRequestHeader();
		p = chdr->encap(just_payload_part);
		p = xiah.encap(p, true);
		delete chdr;

		XID	source_sid = sk->src_path.xid(sk->src_path.destination_node());
		XID	destination_cid = dst_path.xid(dst_path.destination_node());

		XIDpair xid_pair;
		xid_pair.set_src(source_sid);
		xid_pair.set_dst(destination_cid);

		// Map the src & dst XID pair to source port
		XIDpairToSock.set(xid_pair, sk);

		// Store the packet into buffer
		WritablePacket *copy_req_pkt = copy_cid_req_packet(p, sk);
		sk->XIDtoCIDreqPkt.set(destination_cid, copy_req_pkt);

		// Set the status of CID request
		sk->XIDtoStatus.set(destination_cid, WAITING_FOR_CHUNK);

		// Set the status of ReadCID reqeust
		sk->XIDtoReadReq.set(destination_cid, false);

		// Set timer
		Timestamp cid_req_expiry  = Timestamp::now() + Timestamp::make_msec(ACK_DELAY);
		sk->XIDtoExpiryTime.set(destination_cid, cid_req_expiry);
		sk->XIDtoTimerOn.set(destination_cid, true);

		if (! _timer.scheduled() || _timer.expiry() >= cid_req_expiry )
			_timer.reschedule_at(cid_req_expiry);

		portToSock.set(_sport, sk);

		output(NETWORK_PORT).push(p);
	}

	ReturnResult(_sport, xia_socket_msg); // TODO: Error codes?
}



void XTRANSPORT::XgetChunkStatus(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg = xia_socket_msg->mutable_x_getchunkstatus();

	int numCids = x_getchunkstatus_msg->dag_size();
	String pktPayload(x_getchunkstatus_msg->payload().c_str(), x_getchunkstatus_msg->payload().size());

	// send CID-Requests
	for (int i = 0; i < numCids; i++) {
		String dest = x_getchunkstatus_msg->dag(i).c_str();
		XIAPath dst_path;
		dst_path.parse(dest);

		//Find DAG info for this DGRAM
		sock *sk = portToSock.get(_sport);

		XID	destination_cid = dst_path.xid(dst_path.destination_node());

		// Check the status of CID request
		HashTable<XID, int>::iterator it;
		it = sk->XIDtoStatus.find(destination_cid);

		if (it != sk->XIDtoStatus.end()) {
			// There is an entry
			int status = it->second;

			if (status == WAITING_FOR_CHUNK) {
				x_getchunkstatus_msg->add_status("WAITING");

			} else if (status == READY_TO_READ) {
				x_getchunkstatus_msg->add_status("READY");

			} else if (status == INVALID_HASH) {
				x_getchunkstatus_msg->add_status("INVALID_HASH");

			} else if (status == REQUEST_FAILED) {
				x_getchunkstatus_msg->add_status("FAILED");
			}

		} else {
			// Status query for the CID that was not requested...
			x_getchunkstatus_msg->add_status("FAILED");
		}
	}

	// Send back the report

	const char *buf = "CID request status response";
	x_getchunkstatus_msg->set_payload((const char*)buf, strlen(buf) + 1);

	ReturnResult(_sport, xia_socket_msg); // TODO: Error codes?
}



void XTRANSPORT::XreadChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Readchunk_Msg *x_readchunk_msg = xia_socket_msg->mutable_x_readchunk();

	String dest = x_readchunk_msg->dag().c_str();
	WritablePacket *copy;
	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	sock *sk = portToSock.get(_sport);

	XID	destination_cid = dst_path.xid(dst_path.destination_node());

	// Update the status of ReadCID reqeust
	sk->XIDtoReadReq.set(destination_cid, true);
	portToSock.set(_sport, sk);

	// Check the status of CID request
	HashTable<XID, int>::iterator it;
	it = sk->XIDtoStatus.find(destination_cid);

	if (it != sk->XIDtoStatus.end()) {
		// There is an entry
		int status = it->second;

		if (status != READY_TO_READ  &&
			status != INVALID_HASH) {
			// Do nothing

		} else {
			// Send the buffered pkt to upper layer

			sk->XIDtoReadReq.set(destination_cid, false);
			portToSock.set(_sport, sk);

			HashTable<XID, WritablePacket*>::iterator it2;
			it2 = sk->XIDtoCIDresponsePkt.find(destination_cid);
			copy = copy_cid_response_packet(it2->second, sk);

			XIAHeader xiah(copy->xia_header());

			//Unparse dag info
			String src_path = xiah.src_path().unparse();

			xia::X_Readchunk_Msg *x_readchunk_msg = xia_socket_msg->mutable_x_readchunk();
			x_readchunk_msg->set_dag(src_path.c_str());
			x_readchunk_msg->set_payload((const char *)xiah.payload(), xiah.plen());

			DBG(">>send chunk to API after read %d\n", _sport);

			/*
			 * Taking out these lines fixes the problem with getting the same CID
			 * multiple times for subsequent chunks
			 * FIXME: but does it cause us to leak?
			it2->second->kill();
			sk->XIDtoCIDresponsePkt.erase(it2);
			*/

			portToSock.set(_sport, sk);
		}
	}

	ReturnResult(_sport, xia_socket_msg); // TODO: Error codes?
}



void XTRANSPORT::XremoveChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Removechunk_Msg *x_rmchunk_msg = xia_socket_msg->mutable_x_removechunk();

	int32_t contextID = x_rmchunk_msg->contextid();
	String src(x_rmchunk_msg->cid().c_str(), x_rmchunk_msg->cid().size());
	//append local address before CID
	String str_local_addr = _local_addr.unparse_re();
	str_local_addr = "RE " + str_local_addr + " CID:" + src;
	XIAPath src_path;
	src_path.parse(str_local_addr);

	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(HLIM_DEFAULT);
	xiah.set_dst_path(_local_addr);
	xiah.set_src_path(src_path);
	xiah.set_nxt(CLICK_XIA_NXT_CID);

	WritablePacket *just_payload_part = WritablePacket::make(256, (const void*)NULL, 0, 0);

	WritablePacket *p = NULL;
	ContentHeaderEncap  contenth(0, 0, 0, 0, ContentHeader::OP_LOCAL_REMOVECID, contextID);
	p = contenth.encap(just_payload_part);
	p = xiah.encap(p, true);

	DBG("sent remove cid packet to cache");
	output(CACHE_PORT).push(p);

	xia::X_Removechunk_Msg *_msg = xia_socket_msg->mutable_x_removechunk();
	_msg->set_contextid(contextID);
	_msg->set_cid(src.c_str());
	_msg->set_status(0);

	ReturnResult(_sport, xia_socket_msg); // TODO: Error codes?
}



void XTRANSPORT::XputChunk(unsigned short _sport, xia::XSocketMsg *xia_socket_msg)
{
	xia::X_Putchunk_Msg *x_putchunk_msg = xia_socket_msg->mutable_x_putchunk();

	DBG("putchunk message from API %d\n", _sport);

	int32_t contextID = x_putchunk_msg->contextid();
	int32_t ttl = x_putchunk_msg->ttl();
	int32_t cacheSize = x_putchunk_msg->cachesize();
	int32_t cachePolicy = x_putchunk_msg->cachepolicy();

	String pktPayload(x_putchunk_msg->payload().c_str(), x_putchunk_msg->payload().size());
	String src;

	/* Computes SHA1 Hash if user does not supply it */
	unsigned char digest[SHA_DIGEST_LENGTH];
	xs_getSHA1Hash((const unsigned char *)pktPayload.c_str(), \
		pktPayload.length(), digest, SHA_DIGEST_LENGTH);

	char hexBuf[3];
	for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf(hexBuf, "%02x", digest[i]);
		src.append(const_cast<char *>(hexBuf), 2);
	}

	DBG("ctxID=%d, length=%d, ttl=%d cid=%s\n", contextID, x_putchunk_msg->payload().size(), ttl, src.c_str());

	//append local address before CID
	String str_local_addr = _local_addr.unparse_re();
	str_local_addr = "RE " + str_local_addr + " CID:" + src;
	XIAPath src_path;
	src_path.parse(str_local_addr);

	_errh->debug("DAG: %s\n", str_local_addr.c_str());

	/*TODO: The destination dag of the incoming packet is local_addr:XID
	 * Thus the cache thinks it is destined for local_addr and delivers to socket
	 * This must be ignored. Options
	 * 1. Use an invalid SID
	 * 2. The cache should only store the CID responses and not forward them to
	 *	local_addr when the source and the destination HIDs are the same.
	 * 3. Use the socket SID on which putCID was issued. This will
	 *	result in a reply going to the same socket on which the putCID was issued.
	 *	Use the response to return 1 to the putCID call to indicate success.
	 *	Need to add sk/ephemeral SID generation for this to work.
	 * 4. Special OPCODE in content extension header and treat it specially in content module (done below)
	 */

	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(HLIM_DEFAULT);
	xiah.set_dst_path(_local_addr);
	xiah.set_src_path(src_path);
	xiah.set_nxt(CLICK_XIA_NXT_CID);

	//Might need to remove more if another header is required (eg some control/DAG info)

	WritablePacket *just_payload_part = WritablePacket::make(256, (const void*)pktPayload.c_str(), pktPayload.length(), 0);

	WritablePacket *p = NULL;
	int chunkSize = pktPayload.length();
	ContentHeaderEncap  contenth(0, 0, pktPayload.length(), chunkSize, ContentHeader::OP_LOCAL_PUTCID,
								 contextID, ttl, cacheSize, cachePolicy);
	p = contenth.encap(just_payload_part);
	p = xiah.encap(p, true);

	DBG("sent packet to cache");

	output(CACHE_PORT).push(p);

	// (for Ack purpose) Reply with a packet with the destination port=source port
	x_putchunk_msg->set_cid(src.c_str());
	ReturnResult(_sport, xia_socket_msg, 0, 0);
}




void XTRANSPORT::XpushChunkto(unsigned short _sport, xia::XSocketMsg *xia_socket_msg, WritablePacket * /*p_in */)
{
	xia::X_Pushchunkto_Msg *x_pushchunkto_msg = xia_socket_msg->mutable_x_pushchunkto();

	int32_t contextID = x_pushchunkto_msg->contextid();
	int32_t ttl = x_pushchunkto_msg->ttl();
	int32_t cacheSize = x_pushchunkto_msg->cachesize();
	int32_t cachePolicy = x_pushchunkto_msg->cachepolicy();

	String pktPayload(x_pushchunkto_msg->payload().c_str(), x_pushchunkto_msg->payload().size());
	int pktPayloadSize = pktPayload.length();


	// send CID-Requests

	String dest = x_pushchunkto_msg->ddag().c_str();
	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	sock *sk = portToSock.get(_sport);

	if (!sk) {
		//No local SID bound yet, so bind one
		sk = new sock();
	}

	if (sk->initialized == false) {
		sk->initialized = true;
		sk->full_src_dag = true;
		sk->port = _sport;
		String str_local_addr = _local_addr.unparse_re();

		//TODO: AD->HID->SID->CID We can add SID here (AD->HID->SID->CID) if SID is passed or generate randomly
		// Contentmodule forwarding needs to be fixed to get this to work. (wrong comparison there)
		// Since there is no way to dictate policy to cache which content to accept right now this wasn't added.

		sk->src_path.parse_re(str_local_addr);

		XID source_xid = sk->src_path.xid(sk->src_path.destination_node());

		XIDtoSock.set(source_xid, sk);
		addRoute(source_xid);
	}

	// Case of initial binding to only SID
	if (sk->full_src_dag == false) {
		sk->full_src_dag = true;
		String str_local_addr = _local_addr.unparse_re();
		XID front_xid = sk->src_path.xid(sk->src_path.destination_node());
		String xid_string = front_xid.unparse();

		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
		INFO("str_local_addr: %s", str_local_addr.c_str() );
		sk->src_path.parse_re(str_local_addr);
	}

	if (sk->src_path.unparse_re().length() != 0) {
		//Recalculate source path
		XID	source_xid = sk->src_path.xid(sk->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
		INFO("str_local_addr: %s", str_local_addr.c_str() );
		sk->src_path.parse(str_local_addr);
	}

	portToSock.set(_sport, sk);
	if (_sport != sk->port) {
		ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
	}

	sk = portToSock.get(_sport); // why are we refetching???

	DBG("sent packet to %s, from %s\n", dest.c_str(), sk->src_path.unparse_re().c_str());

	INFO("PUSHCID: %s", x_pushchunkto_msg->cid().c_str());
	String src(x_pushchunkto_msg->cid().c_str(), x_pushchunkto_msg->cid().size());
	//append local address before CID
	String cid_str_local_addr = sk->src_path.unparse_re();
	cid_str_local_addr = "RE " + cid_str_local_addr + " CID:" + src;
	XIAPath cid_src_path;
	cid_src_path.parse(cid_str_local_addr);
	INFO("cid_local_addr: %s", cid_str_local_addr.c_str() );


	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_nxt(CLICK_XIA_NXT_CID);
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(sk->hlim);
	xiah.set_dst_path(dst_path);
	xiah.set_src_path(cid_src_path); //FIXME: is this the correct way to do it? Do we need SID? AD->HID->SID->CID
	xiah.set_plen(pktPayloadSize);

// 	WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_pushchunkto_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());
	WritablePacket *just_payload_part = WritablePacket::make(256, (const void*)x_pushchunkto_msg->payload().c_str(), pktPayloadSize, 0);

	WritablePacket *p = NULL;

	//Add Content header
// 	ContentHeaderEncap *chdr = ContentHeaderEncap::MakePushHeader();
	int chunkSize = pktPayload.length();
	ContentHeaderEncap  contenth(0, 0, pktPayload.length(), chunkSize, ContentHeader::OP_PUSH,
								 contextID, ttl, cacheSize, cachePolicy);

	p = contenth.encap(just_payload_part);
	p = xiah.encap(p, true);

// 	XID	source_sid = sk->src_path.xid(sk->src_path.destination_node());
// 	XID	destination_cid = dst_path.xid(dst_path.destination_node());
	//FIXME this is wrong
	XID	source_cid = cid_src_path.xid(cid_src_path.destination_node());
// 	XID	source_cid = sk->src_path.xid(cid_src_path.destination_node());
	XID	destination_sid = dst_path.xid(dst_path.destination_node());

	XIDpair xid_pair;
	xid_pair.set_src(source_cid);
	xid_pair.set_dst(destination_sid);

	// Map the src & dst XID pair to source port
	XIDpairToSock.set(xid_pair, sk);

	portToSock.set(_sport, sk);
	if (_sport != sk->port) {
		ERROR("ERROR _sport %d, sk->port %d", _sport, sk->port);
	}

	output(NETWORK_PORT).push(p);
	ReturnResult(_sport, xia_socket_msg, 0, 0);
}
#endif

CLICK_ENDDECLS

EXPORT_ELEMENT(XTRANSPORT)
ELEMENT_REQUIRES(userlevel)
ELEMENT_MT_SAFE(XTRANSPORT)
ELEMENT_LIBS(-lcrypto -lssl -lprotobuf)
