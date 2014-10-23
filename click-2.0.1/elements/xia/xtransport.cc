#include "../../userlevel/xia.pb.h"
#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>

#include <click/xiacontentheader.hh>
#include "xiatransport.hh"
#include "xtransport.hh"
#include <click/xiatransportheader.hh>

#include <fstream>



/*
** FIXME:
** - why is xia_socket_msg in the class definition and not a local variable?
** - implement a backoff delay on retransmits so we don't flood the connection
** - fix cid header size issue so we work correctly with the linux version
** - migrate from uisng printf and click_chatter to using the click ErrorHandler class
** - there are still some small memory leaks happening when stream sockets are created/used/closed
**   (problem does not happen if sockets are just opened and closed)
** - fix issue in SYN code with XIDPairToConnectPending (see comment in code for details)
** - what is the constant 22 for near line 850? I can't find a 22 anywhere else in the source tree
*/

#define DEBUG 0

CLICK_DECLS

XTRANSPORT::XTRANSPORT()
	: _timer(this)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	_id = 0;
	isConnected = false;

	_ackdelay_ms = ACK_DELAY;
	_teardown_wait_ms = TEARDOWN_DELAY;

//	pthread_mutexattr_init(&_lock_attr);
//	pthread_mutexattr_settype(&_lock_attr, PTHREAD_MUTEX_RECURSIVE);
//	pthread_mutex_init(&_lock, &_lock_attr);

	cp_xid_type("SID", &_sid_type); 
}


int
XTRANSPORT::configure(Vector<String> &conf, ErrorHandler *errh)
{
	XIAPath local_addr;
	XID local_4id;
	Element* routing_table_elem;
	bool is_dual_stack_router;
	_is_dual_stack_router = false;


	if (cp_va_kparse(conf, this, errh,
					 "LOCAL_ADDR", cpkP + cpkM, cpXIAPath, &local_addr,
					 "LOCAL_4ID", cpkP + cpkM, cpXID, &local_4id,
					 "ROUTETABLENAME", cpkP + cpkM, cpElement, &routing_table_elem,
					 "IS_DUAL_STACK_ROUTER", 0, cpBool, &is_dual_stack_router,
					 cpEnd) < 0)
		return -1;

	_local_addr = local_addr;
	_local_hid = local_addr.xid(local_addr.destination_node());
	_local_4id = local_4id;
	// IP:0.0.0.0 indicates NULL 4ID
	_null_4id.parse("IP:0.0.0.0");

	_is_dual_stack_router = is_dual_stack_router;

	/*
	// If a valid 4ID is given, it is included (as a fallback) in the local_addr
	if(_local_4id != _null_4id) {
		String str_local_addr = _local_addr.unparse();
		size_t AD_found_start = str_local_addr.find_left("AD:");
		size_t AD_found_end = str_local_addr.find_left(" ", AD_found_start);
		String AD_str = str_local_addr.substring(AD_found_start, AD_found_end - AD_found_start);
		String HID_str = _local_hid.unparse();
		String IP4ID_str = _local_4id.unparse();
		String new_local_addr = "RE ( " + IP4ID_str + " ) " + AD_str + " " + HID_str;
		//click_chatter("new address is - %s", new_local_addr.c_str());
		_local_addr.parse(new_local_addr);	
	}
	*/					

#if USERLEVEL
	_routeTable = dynamic_cast<XIAXIDRouteTable*>(routing_table_elem);
#else
	_routeTable = reinterpret_cast<XIAXIDRouteTable*>(routing_table_elem);
#endif

	return 0;
}

XTRANSPORT::~XTRANSPORT()
{
	//Clear all hashtable entries
	XIDtoPort.clear();
	XIDtoPushPort.clear();
	portToDAGinfo.clear();
	XIDpairToPort.clear();
	XIDpairToConnectPending.clear();

	hlim.clear();
	xcmp_listeners.clear();
	nxt_xport.clear();

//	pthread_mutex_destroy(&_lock);
//	pthread_mutexattr_destroy(&_lock_attr);
}


int
XTRANSPORT::initialize(ErrorHandler *)
{
	_timer.initialize(this);
	//_timer.schedule_after_msec(1000);
	//_timer.unschedule();
	return 0;
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

void
XTRANSPORT::run_timer(Timer *timer)
{
//	pthread_mutex_lock(&_lock);

	assert(timer == &_timer);

	Timestamp now = Timestamp::now();
	Timestamp earlist_pending_expiry = now;

	WritablePacket *copy;

	bool tear_down;

	for (HashTable<unsigned short, DAGinfo>::iterator iter = portToDAGinfo.begin(); iter != portToDAGinfo.end(); ++iter ) {
		unsigned short _sport = iter->first;
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);
		tear_down = false;

		// check if pending
		if (daginfo->timer_on == true) {
			// check if synack waiting
			if (daginfo->synack_waiting == true && daginfo->expiry <= now ) {
				//click_chatter("Timer: synack waiting\n");
		
				if (daginfo->num_connect_tries <= MAX_CONNECT_TRIES) {

					//click_chatter("Timer: SYN RETRANSMIT! \n");
					copy = copy_packet(daginfo->syn_pkt, daginfo);
					// retransmit syn
					XIAHeader xiah(copy);
					// printf("Timer: (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), (char *)xiah.payload(), xiah.plen());
					output(NETWORK_PORT).push(copy);

					daginfo->timer_on = true;
					daginfo->synack_waiting = true;
					daginfo->expiry = now + Timestamp::make_msec(_ackdelay_ms);
					daginfo->num_connect_tries++;

				} else {
					// Stop sending the connection request & Report the failure to the application
					daginfo->timer_on = false;
					daginfo->synack_waiting = false;

					String str = String("^Connection-failed^");
					WritablePacket *ppp = WritablePacket::make (256, str.c_str(), str.length(), 0);

					if (DEBUG)
						//click_chatter("Timer: Sent packet to socket with port %d", _sport);
                        output(API_PORT).push(UDPIPPrep(ppp, _sport));
				}
			} else if (daginfo->migrateack_waiting == true && daginfo->expiry <= now ) {
				//click_chatter("Timer: migrateack waiting\n");
				if (daginfo->num_migrate_tries <= MAX_CONNECT_TRIES) {

					//click_chatter("Timer: SYN RETRANSMIT! \n");
					copy = copy_packet(daginfo->migrate_pkt, daginfo);
					// retransmit migrate
					XIAHeader xiah(copy);
					// printf("Timer: (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), (char *)xiah.payload(), xiah.plen());
					output(NETWORK_PORT).push(copy);

					daginfo->timer_on = true;
					daginfo->migrateack_waiting = true;
					daginfo->expiry = now + Timestamp::make_msec(_ackdelay_ms);
					daginfo->num_migrate_tries++;
				} else {
					// Stop sending the connection request & Report the failure to the application
					daginfo->timer_on = false;
					daginfo->migrateack_waiting = false;

					String str = String("^Connection-failed^");
					WritablePacket *ppp = WritablePacket::make (256, str.c_str(), str.length(), 0);

					if (DEBUG) {
						//click_chatter("Timer: Sent packet to socket with port %d", _sport);
                        output(API_PORT).push(UDPIPPrep(ppp, _sport));
					}
				}
			} else if (daginfo->dataack_waiting == true && daginfo->expiry <= now ) {

				// adding check to see if anything was retransmitted. We can get in here with
				// no packets in the daginfo->sent_pkt array waiting to go and will stay here forever
				bool retransmit_sent = false;

				if (daginfo->num_retransmit_tries < MAX_RETRANSMIT_TRIES) {

				//click_chatter("Timer: DATA RETRANSMIT at from (%s) from_port=%d base=%d next_seq=%d \n\n", (_local_addr.unparse()).c_str(), _sport, daginfo->base, daginfo->next_seqnum );
		
					// retransmit data
					for (unsigned int i = daginfo->base; i < daginfo->next_seqnum; i++) {
						if (daginfo->sent_pkt[i % MAX_WIN_SIZE] != NULL) {
							copy = copy_packet(daginfo->sent_pkt[i % MAX_WIN_SIZE], daginfo);
							XIAHeader xiah(copy);
							//printf("Timer: (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), (char *)xiah.payload(), xiah.plen());
							//printf("pusing the retransmit pkt\n");
							output(NETWORK_PORT).push(copy);
							retransmit_sent = true;
						}
					}
				} else {
					//printf("retransmit counter exceeded\n");
					// FIXME what cleanup should happen here?
					// should we do a NAK?
				}

				if (retransmit_sent) {
					//click_chatter("resetting retransmit timer for %d\n", _sport);
					daginfo->timer_on = true;
					daginfo->dataack_waiting = true;
					daginfo-> num_retransmit_tries++;
					daginfo->expiry = now + Timestamp::make_msec(_ackdelay_ms);
				} else {
					//click_chatter("terminating retransmit timer for %d\n", _sport);
					daginfo->timer_on = false;
					daginfo->dataack_waiting = false;
					daginfo->num_retransmit_tries = 0;
				}

			} else if (daginfo->teardown_waiting == true && daginfo->teardown_expiry <= now) {
				tear_down = true;
				daginfo->timer_on = false;
				portToActive.set(_sport, false);

				//XID source_xid = portToDAGinfo.get(_sport).xid;

				// this check for -1 prevents a segfault cause by bad XIDs
				// it may happen in other cases, but opening a XSOCK_STREAM socket, calling
				// XreadLocalHostAddr and then closing the socket without doing anything else will
				// cause the problem
				// TODO: make sure that -1 is the only condition that will cause us to get a bad XID
				if (daginfo->src_path.destination_node() != -1) {
					XID source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
					if (!daginfo->isAcceptSocket) {

						//click_chatter("deleting route %s from port %d\n", source_xid.unparse().c_str(), _sport);
						delRoute(source_xid);
						XIDtoPort.erase(source_xid);
					}
				}

				portToDAGinfo.erase(_sport);
				portToActive.erase(_sport);
				hlim.erase(_sport);

				nxt_xport.erase(_sport);
				xcmp_listeners.remove(_sport);
				for (int i = 0; i < MAX_WIN_SIZE; i++) {
					if (daginfo->sent_pkt[i] != NULL) {
						daginfo->sent_pkt[i]->kill();
						daginfo->sent_pkt[i] = NULL;
					}
				}
			}
		}

		if (tear_down == false) {

			// find the (next) earlist expiry
			if (daginfo->timer_on == true && daginfo->expiry > now && ( daginfo->expiry < earlist_pending_expiry || earlist_pending_expiry == now ) ) {
				earlist_pending_expiry = daginfo->expiry;
			}
			if (daginfo->timer_on == true && daginfo->teardown_expiry > now && ( daginfo->teardown_expiry < earlist_pending_expiry || earlist_pending_expiry == now ) ) {
				earlist_pending_expiry = daginfo->teardown_expiry;
			}


			// check for CID request cases
			for (HashTable<XID, bool>::iterator it = daginfo->XIDtoTimerOn.begin(); it != daginfo->XIDtoTimerOn.end(); ++it ) {
				XID requested_cid = it->first;
				bool timer_on = it->second;

				HashTable<XID, Timestamp>::iterator it2;
				it2 = daginfo->XIDtoExpiryTime.find(requested_cid);
				Timestamp cid_req_expiry = it2->second;

				if (timer_on == true && cid_req_expiry <= now) {
					//printf("CID-REQ RETRANSMIT! \n");
					//retransmit cid-request
					HashTable<XID, WritablePacket*>::iterator it3;
					it3 = daginfo->XIDtoCIDreqPkt.find(requested_cid);
					copy = copy_cid_req_packet(it3->second, daginfo);
					XIAHeader xiah(copy);
					//printf("\n\n (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), (char *)xiah.payload(), xiah.plen());
					output(NETWORK_PORT).push(copy);

					cid_req_expiry  = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);
					daginfo->XIDtoExpiryTime.set(requested_cid, cid_req_expiry);
					daginfo->XIDtoTimerOn.set(requested_cid, true);
				}

				if (timer_on == true && cid_req_expiry > now && ( cid_req_expiry < earlist_pending_expiry || earlist_pending_expiry == now ) ) {
					earlist_pending_expiry = cid_req_expiry;
				}
			}

			portToDAGinfo.set(_sport, *daginfo);
		}
	}

	// Set the next timer
	if (earlist_pending_expiry > now) {
		_timer.reschedule_at(earlist_pending_expiry);
	}

//	pthread_mutex_unlock(&_lock);
}

void
XTRANSPORT::copy_common(DAGinfo *daginfo, XIAHeader &xiahdr, XIAHeaderEncap &xiah) {  

	//Recalculate source path
	XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
	String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse();
	//Make source DAG _local_addr:SID
	String dagstr = daginfo->src_path.unparse_re();

	//Client Mobility...
	if (dagstr.length() != 0 && dagstr != str_local_addr) {
		//Moved!
		// 1. Update 'daginfo->src_path'
		daginfo->src_path.parse_re(str_local_addr);
	}

	xiah.set_nxt(xiahdr.nxt());
	xiah.set_last(xiahdr.last());
	xiah.set_hlim(xiahdr.hlim());
	xiah.set_dst_path(daginfo->dst_path);
	xiah.set_src_path(daginfo->src_path);
	xiah.set_plen(xiahdr.plen());
}

WritablePacket *
XTRANSPORT::copy_packet(Packet *p, DAGinfo *daginfo) {  

	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;
	copy_common(daginfo, xiahdr, xiah);

	TransportHeader thdr(p);
	TransportHeaderEncap *new_thdr = new TransportHeaderEncap(thdr.type(), thdr.pkt_info(), thdr.seq_num(), thdr.ack_num(), thdr.length());

	WritablePacket *copy = WritablePacket::make(256, thdr.payload(), xiahdr.plen() - thdr.hlen(), 20);

	copy = new_thdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete new_thdr;

	return copy;
}


WritablePacket *
XTRANSPORT::copy_cid_req_packet(Packet *p, DAGinfo *daginfo) {

	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;
	copy_common(daginfo, xiahdr, xiah);

	WritablePacket *copy = WritablePacket::make(256, xiahdr.payload(), xiahdr.plen(), 20);

	ContentHeaderEncap *chdr = ContentHeaderEncap::MakeRequestHeader();

	copy = chdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete chdr;
	xiah.set_plen(xiahdr.plen());

	return copy;
}


WritablePacket *
XTRANSPORT::copy_cid_response_packet(Packet *p, DAGinfo *daginfo) {

	XIAHeader xiahdr(p);
	XIAHeaderEncap xiah;
	copy_common(daginfo, xiahdr, xiah);

	WritablePacket *copy = WritablePacket::make(256, xiahdr.payload(), xiahdr.plen(), 20);

	ContentHeader chdr(p);
	ContentHeaderEncap *new_chdr = new ContentHeaderEncap(chdr.opcode(), chdr.chunk_offset(), chdr.length());

	copy = new_chdr->encap(copy);
	copy = xiah.encap(copy, false);
	delete new_chdr;
	xiah.set_plen(xiahdr.plen());

	return copy;
}

void XTRANSPORT::ProcessAPIPacket(WritablePacket *p_in)
{
		
	//Extract the destination port
	unsigned short _sport = SRC_PORT_ANNO(p_in);

// 	if (DEBUG)
//         click_chatter("\nPush: Got packet from API sport:%d",ntohs(_sport));

	std::string p_buf;
	p_buf.assign((const char*)p_in->data(), (const char*)p_in->end_data());

	//protobuf message parsing
	xia_socket_msg.ParseFromString(p_buf);
	

	switch(xia_socket_msg.type()) {
	case xia::XSOCKET:
		Xsocket(_sport);
		break;
	case xia::XSETSOCKOPT:
		Xsetsockopt(_sport);
		break;
	case xia::XGETSOCKOPT:
		Xgetsockopt(_sport);
		break;
	case xia::XBIND:
		Xbind(_sport);
		break;
	case xia::XCLOSE:
		Xclose(_sport);
		break;
	case xia::XCONNECT:
		Xconnect(_sport);
		break;
	case xia::XACCEPT:
		Xaccept(_sport);
		break;
	case xia::XCHANGEAD:
		Xchangead(_sport);
		break;
	case xia::XREADLOCALHOSTADDR:
		Xreadlocalhostaddr(_sport);
		break;
	case xia::XUPDATENAMESERVERDAG:
		Xupdatenameserverdag(_sport);
		break;
	case xia::XREADNAMESERVERDAG:
		Xreadnameserverdag(_sport);
		break;
	case xia::XISDUALSTACKROUTER:
		Xisdualstackrouter(_sport);	
		break;
	case xia::XSEND:
		Xsend(_sport, p_in);
		break;
	case xia::XSENDTO:
		Xsendto(_sport, p_in);
		break;
	case xia::XREQUESTCHUNK:
		XrequestChunk(_sport, p_in);
		break;
	case xia::XGETCHUNKSTATUS:
		XgetChunkStatus(_sport);
		break;
	case xia::XREADCHUNK:
		XreadChunk(_sport);
		break;
	case xia::XREMOVECHUNK:
		XremoveChunk(_sport);
		break;
	case xia::XPUTCHUNK:
		XputChunk(_sport);
		break;
	case xia::XGETPEERNAME:
		Xgetpeername(_sport);
		break;
	case xia::XGETSOCKNAME:
		Xgetsockname(_sport);
		break;
	case xia::XPUSHCHUNKTO:
		XpushChunkto(_sport, p_in);
		break;
	case xia::XBINDPUSH:
		XbindPush(_sport);
		break;
	default:
		click_chatter("\n\nERROR: API TRAFFIC !!!\n\n");
		break;
	}

	p_in->kill();
}

void XTRANSPORT::ProcessNetworkPacket(WritablePacket *p_in)
{
// 	if (DEBUG)
// 		click_chatter("Got packet from network");

	//Extract the SID/CID
	XIAHeader xiah(p_in->xia_header());
	XIAPath dst_path = xiah.dst_path();
	XID _destination_xid(xiah.hdr()->node[xiah.last()].xid);
	//TODO:In case of stream use source AND destination XID to find port, if not found use source. No TCP like protocol exists though
	//TODO:pass dag back to recvfrom. But what format?

	XIAPath src_path = xiah.src_path();
	XID	_source_xid = src_path.xid(src_path.destination_node());
	
	//click_chatter("NetworkPacket, Src: %s, Dest: %s", xiah.dst_path().unparse().c_str(), xiah.src_path().unparse().c_str());

	unsigned short _dport = XIDtoPort.get(_destination_xid);  // This is to be updated for the XSOCK_STREAM type connections below

	bool sendToApplication = true;
	//String pld((char *)xiah.payload(), xiah.plen());
	//printf("\n\n 1. (%s) Received=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), pld.c_str(), xiah.plen());

	TransportHeader thdr(p_in);

	if (xiah.nxt() == CLICK_XIA_NXT_XCMP) {
		// FIXME: This shouldn't strip off the header. raw sockets return raw packets. 
		// (Matt): I've tweaked this to work properly
		// strip off the header and make a writable packet

		String src_path = xiah.src_path().unparse();
		String header((const char*)xiah.hdr(), xiah.hdr_size());
		String payload((const char*)xiah.payload(), xiah.plen());//+xiah.hdr_size());
		//String payload((const char*)thdr.payload(), xiah.plen() - thdr.hlen());
		String str = header + payload;

		xia::XSocketMsg xsm;
		xsm.set_type(xia::XRECV);
		xia::X_Recv_Msg *x_recv_msg = xsm.mutable_x_recv();
		x_recv_msg->set_dag(src_path.c_str());
		x_recv_msg->set_payload(str.c_str(), str.length());

		std::string p_buf;
		xsm.SerializeToString(&p_buf);

		WritablePacket *xcmp_pkt = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

		list<int>::iterator i;

		for (i = xcmp_listeners.begin(); i != xcmp_listeners.end(); i++) {
			int port = *i;
			output(API_PORT).push(UDPIPPrep(xcmp_pkt, port));
		}

		return;

	} else if (thdr.type() == TransportHeader::XSOCK_STREAM) {
		// Is this packet arriving at a rendezvous server?
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);
		if (daginfo->sock_type == SOCK_RAW) {
			String src_path = xiah.src_path().unparse();
			String dst_path = xiah.dst_path().unparse();
			click_chatter("ProcessNetworkPacket: received stream packet on raw socket");
			click_chatter("ProcessNetworkPacket: src|%s|", src_path.c_str());
			click_chatter("ProcessNetworkPacket: dst|%s|", dst_path.c_str());
			click_chatter("ProcessNetworkPacket: len=%d", p_in->length());
			xia::XSocketMsg xsm;
			xsm.set_type(xia::XRECV);
			xia::X_Recv_Msg *x_recv_msg = xsm.mutable_x_recv();
			x_recv_msg->set_dag(src_path.c_str());
			// Include entire packet (including headers) as payload for API
			x_recv_msg->set_payload(p_in->data(), p_in->length());
			std::string p_buf;
			xsm.SerializeToString(&p_buf);
			WritablePacket *raw_pkt = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			click_chatter("ProcessNetworkPacket: delivering packet to raw socket");
			output(API_PORT).push(UDPIPPrep(raw_pkt, _dport));
			return;
		}

		//printf("stream socket dport = %d\n", _dport);
		if (thdr.pkt_info() == TransportHeader::SYN) {
			//printf("syn dport = %d\n", _dport);
			// Connection request from client...

			click_chatter("ProcessNetworkPacket: SYN: Source: %s, Dest: %s", _source_xid.unparse().c_str(), _destination_xid.unparse().c_str());
			// First, check if this request is already in the pending queue
			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			HashTable<XIDpair , bool>::iterator it;
			it = XIDpairToConnectPending.find(xid_pair);

			// FIXME:
			// XIDpairToConnectPending never gets cleared, and will cause problems if matching XIDs
			// were used previously. Commenting out the check for now. Need to look into whether
			// or not we can just get rid of this logic? probably neede for retransmit cases
			// if needed, where should it be cleared???
			if (1) {
//			if (it == XIDpairToConnectPending.end()) {
				// if this is new request, put it in the queue

				// Todo: 1. prepare new Daginfo and store it
				//	 2. send SYNACK to client
				//	   3. Notify the api of SYN reception

				//1. Prepare new DAGinfo for this connection
				DAGinfo daginfo;
				daginfo.port = -1; // just for now. This will be updated via Xaccept call

				daginfo.sock_type = SOCK_STREAM;

				daginfo.dst_path = src_path;
				click_chatter("ProcessNetworkPacket: SYN: Daginfo.dst_path=%s", daginfo.dst_path.unparse().c_str());
				daginfo.src_path = dst_path;
				click_chatter("ProcessNetworkPacket: SYN: Daginfo.src_path=%s", daginfo.src_path.unparse().c_str());
				daginfo.isConnected = true;
				daginfo.initialized = true;
				daginfo.nxt = LAST_NODE_DEFAULT;
				daginfo.last = LAST_NODE_DEFAULT;
				daginfo.hlim = HLIM_DEFAULT;
				daginfo.next_seqnum = 0;
				daginfo.ack_num = 0;

				pending_connection_buf.push(daginfo);

				// Mark these src & dst XID pair
				XIDpairToConnectPending.set(xid_pair, true);

				//portToDAGinfo.set(-1, daginfo);	// just for now. This will be updated via Xaccept call

			} else {
				// If already in the pending queue, just send back SYNACK to the requester
			
				// if this case is hit, we won't trigger the accept, and the connection will get be left
				// in limbo. see above for whether or not we should even be checking.
				// printf("%06d conn request already pending\n", _dport);
				sendToApplication = false;
			}


			//2. send SYNACK to client
			//Add XIA headers
			XIAHeaderEncap xiah_new;
			xiah_new.set_nxt(CLICK_XIA_NXT_TRN);
			xiah_new.set_last(LAST_NODE_DEFAULT);
			xiah_new.set_hlim(HLIM_DEFAULT);
			xiah_new.set_dst_path(src_path);
			xiah_new.set_src_path(dst_path);

			const char* dummy = "Connection_granted";
			WritablePacket *just_payload_part = WritablePacket::make(256, dummy, strlen(dummy), 0);

			WritablePacket *p = NULL;

			xiah_new.set_plen(strlen(dummy));
			//click_chatter("Sent packet to network");

			TransportHeaderEncap *thdr_new = TransportHeaderEncap::MakeSYNACKHeader( 0, 0, 0); // #seq, #ack, length
			p = thdr_new->encap(just_payload_part);

			thdr_new->update();
			xiah_new.set_plen(strlen(dummy) + thdr_new->hlen()); // XIA payload = transport header + transport-layer data

			p = xiah_new.encap(p, false);

			delete thdr_new;
			XIAHeader xiah1(p);
			//String pld((char *)xiah1.payload(), xiah1.plen());
			//printf("\n\n (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), pld.c_str(), xiah1.plen());
			output(NETWORK_PORT).push(p);


			// 3. Notify the api of SYN reception
			//   Done below (via port#5005)

		} else if (thdr.pkt_info() == TransportHeader::SYNACK) {
			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			// Get the dst port from XIDpair table
			_dport = XIDpairToPort.get(xid_pair);

			DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);

			if(!daginfo->synack_waiting) {
				// Fix for synack storm sending messages up to the API
				// still need to fix the root cause, but this prevents the API from 
				// getting multiple connection granted messages
				sendToApplication = false;
			}

			// Clear timer
			daginfo->timer_on = false;
			daginfo->synack_waiting = false;
			// Mobility: Rendezvous updates destDAG mid-flight, so update here.
			daginfo->dst_path = src_path;
			//daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);
		} else if (thdr.pkt_info() == TransportHeader::MIGRATE) {

			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			// Application does not need to know about migration
			sendToApplication = false;

			// Get the dst port from XIDpair table
			_dport = XIDpairToPort.get(xid_pair);

			HashTable<unsigned short, bool>::iterator it1;
			it1 = portToActive.find(_dport);

			if(it1 != portToActive.end() ) {

				DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);

				/*
				if (thdr.seq_num() == daginfo->expected_seqnum) {
					daginfo->expected_seqnum++;
					//printf("(%s) Accept Received data (now expected seq=%d)\n", (_local_addr.unparse()).c_str(), daginfo->expected_seqnum);
				} else {
					sendToApplication = false;
					printf("expected sequence # %d, received %d\n", daginfo->expected_seqnum, thdr.seq_num());
					printf("(%s) Discarded Received data\n", (_local_addr.unparse()).c_str());
				}
				*/

				// Verify the MIGRATE request and start using new DAG
				// No need to wait for an ACK because the operation is idempotent
				// 1. Retrieve payload (srcDAG, destDAG, seqnum) Signature, Pubkey
				const uint8_t *payload = thdr.payload();
				//int payload_len = xiah.plen() - thdr.hlen();
				const uint8_t *payloadptr = payload;
				String remote_DAG((char *)payloadptr, strlen((char *) payloadptr));
				payloadptr += strlen((char *)payloadptr) + 1;
				click_chatter("ProcessNetworkPacket: MIGRATE: remote DAG: %s", remote_DAG.c_str());
				String my_DAG((char *)payloadptr, strlen((char *) payloadptr));
				payloadptr += strlen((char *)payloadptr) + 1;
				click_chatter("ProcessNetworkPacket: MIGRATE: my DAG: %s", my_DAG.c_str());
				String timestamp((char *)payloadptr, strlen((char *) payloadptr));
				payloadptr += strlen((char *)payloadptr) + 1;
				click_chatter("ProcessNetworkPacket: MIGRATE: Timestamp: %s", timestamp.c_str());
				uint16_t siglen;
				memcpy(&siglen, payloadptr, sizeof(uint16_t));
				payloadptr += sizeof(uint16_t);
				click_chatter("ProcessNetworkPacket: MIGRATE: Signature length: %d", siglen);
				uint8_t *signature = (uint8_t *) calloc(siglen, 1);
				memcpy(signature, payloadptr, siglen);
				payloadptr += siglen;
				uint16_t pubkeylen;
				memcpy(&pubkeylen, payloadptr, sizeof(uint16_t));
				payloadptr += sizeof(uint16_t);
				click_chatter("ProcessNetworkPacket: MIGRATE: Pubkey length: %d", pubkeylen);
				char *pubkey = (char *) calloc(pubkeylen, 1);
				memcpy(pubkey, payloadptr, pubkeylen);
				click_chatter("ProcessNetworkPacket: MIGRATE: Pubkey:%s:", pubkey);
				payloadptr += pubkeylen;
				click_chatter("ProcessNetworkPacket: MIGRATE: pkt len: %d", payloadptr - payload);

				// 2. Verify hash of pubkey matches srcDAG destination node
				XIAPath src_path;
				if(src_path.parse(remote_DAG) == false) {
					click_chatter("ProcessNetworkPacket: MIGRATE: ERROR parsing remote DAG:%s:", remote_DAG.c_str());
				}
				String src_SID_string = src_path.xid(src_path.destination_node()).unparse();
				const char *sourceSID = xs_XIDHash(src_SID_string.c_str());
				uint8_t pubkeyhash[SHA_DIGEST_LENGTH];
				char pubkeyhash_hexdigest[XIA_SHA_DIGEST_STR_LEN];
				xs_getPubkeyHash(pubkey, pubkeyhash, sizeof pubkeyhash);
				xs_hexDigest(pubkeyhash, sizeof pubkeyhash, pubkeyhash_hexdigest, sizeof pubkeyhash_hexdigest);
				if(strcmp(pubkeyhash_hexdigest, sourceSID) != 0) {
					click_chatter("ProcessNetworkPacket: ERROR: MIGRATE pubkey hash: %s SourceSID: %s", pubkeyhash_hexdigest, sourceSID);
				}
				click_chatter("ProcessNetworkPacket: MIGRATE: Source SID matches pubkey hash");

				// 3. Verify Signature using Pubkey
				size_t signed_datalen = remote_DAG.length() + my_DAG.length() + timestamp.length() + 3;
				if(!xs_isValidSignature(payload, signed_datalen, signature, siglen, pubkey, pubkeylen)) {
					click_chatter("ProcessNetworkPacket: ERROR: MIGRATE with invalid signature");
				}
				free(signature);
				free(pubkey);
				click_chatter("ProcessNetworkPacket: MIGRATE: Signature validated");

				// 4. Update DAGinfo dst_path with srcDAG
				click_chatter("***EWA: Changing partner path due to MIGRATE: From %s to %s", daginfo->dst_path.unparse().c_str(), src_path.unparse().c_str());
				daginfo->dst_path = src_path;
				daginfo->isConnected = true;
				daginfo->initialized = true;

				// 5. Return MIGRATEACK to notify mobile host of change
				// Construct the payload - 'data'
				// For now (timestamp) signature, Pubkey
				uint8_t *data;
				uint8_t *dataptr;
				uint32_t maxdatalen;
				uint32_t datalen;
				char mypubkey[MAX_PUBKEY_SIZE];
				uint16_t mypubkeylen = MAX_PUBKEY_SIZE;

				click_chatter("ProcessNetworkPacket: MIGRATE: building MIGRATEACK");
				XID my_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK get pubkey for:%s:", my_xid.unparse().c_str());
				if(xs_getPubkey(my_xid.unparse().c_str(), mypubkey, &mypubkeylen)) {
					click_chatter("ProcessNetworkPacket: ERROR: getting Pubkey for MIGRATEACK");
				}
				maxdatalen = remote_DAG.length() + 1 + timestamp.length() + 1 + MAX_SIGNATURE_SIZE + sizeof(uint16_t) + mypubkeylen;
				data = (uint8_t *) calloc(maxdatalen, 1);
				if(data == NULL) {
					click_chatter("ProcessNetworkPacket: ERROR allocating memory for MIGRATEACK");
				}
				dataptr = data;

				// Insert the mobile host DAG whose migration has been accepted
				strcpy((char *)dataptr, remote_DAG.c_str());
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK remoteDAG: %s", (char *)dataptr);
				dataptr += remote_DAG.length() + 1; // null-terminated string

				// Insert timestamp into payload
				strcpy((char *)dataptr, timestamp.c_str());
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK timestamp: %s", (char *)dataptr);
				dataptr += timestamp.length() + 1; // null-terminated string

				// Sign(mobileDAG, Timestamp)
				uint8_t mysignature[MAX_SIGNATURE_SIZE];
				uint16_t mysiglen = MAX_SIGNATURE_SIZE;
				if(xs_sign(my_xid.unparse().c_str(), data, dataptr-data, mysignature, &mysiglen)) {
					click_chatter("ProcessNetworkPacket: ERROR signing MIGRATEACK");
				}

				// Signature length
				memcpy(dataptr, &mysiglen, sizeof(uint16_t));
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK siglen: %d", mysiglen);
				dataptr += sizeof(uint16_t);

				// Signature
				memcpy(dataptr, mysignature, mysiglen);
				dataptr += mysiglen;

				// Public key length
				memcpy(dataptr, &mypubkeylen, sizeof(uint16_t));
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK pubkeylen: %d", mypubkeylen);
				dataptr += sizeof(uint16_t);

				// Public key
				memcpy(dataptr, mypubkey, mypubkeylen);
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK pubkey:%s:", dataptr);
				dataptr += mypubkeylen;

				// Total payload length
				datalen = dataptr - data;
				click_chatter("ProcessNetworkPacket: MIGRATE: MIGRATEACK len: %d", datalen);

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
				//click_chatter("Sent packet to network");

				TransportHeaderEncap *thdr_new = TransportHeaderEncap::MakeMIGRATEACKHeader( 0, 0, 0); // #seq, #ack, length
				p = thdr_new->encap(just_payload_part);

				thdr_new->update();
				xiah_new.set_plen(datalen + thdr_new->hlen()); // XIA payload = transport header + transport-layer data

				p = xiah_new.encap(p, false);

				delete thdr_new;
				output(NETWORK_PORT).push(p);


				// 6. Notify the api of MIGRATE reception
				//   Do we need to? -Nitin
			} else {
				printf("ProcessNetworkPacket: ERROR: Migrating non-existent or inactive session\n");
			}
		} else if (thdr.pkt_info() == TransportHeader::MIGRATEACK) {
			sendToApplication = false;

			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			// Get the dst port from XIDpair table
			_dport = XIDpairToPort.get(xid_pair);

			HashTable<unsigned short, bool>::iterator it1;
			it1 = portToActive.find(_dport);

			if(it1 != portToActive.end() ) {
				DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);

				// Verify the MIGRATEACK and start using new DAG
				// 1. Retrieve payload (migratedDAG, timestamp) signature, Pubkey
				const uint8_t *payload = thdr.payload();
				int payload_len = xiah.plen() - thdr.hlen();
				const uint8_t *payloadptr = payload;
				size_t signed_datalen;

				// Extract the migrated DAG that the fixed host accepted
				String migrated_DAG((char *)payloadptr, strlen((char *) payloadptr));
				payloadptr += strlen((char *)payloadptr) + 1;
				click_chatter("ProcessNetworkPacket: MIGRATEACK: migrated DAG: %s", migrated_DAG.c_str());

				// Extract the timestamp corresponding to the migration message that was sent
				// Helps handle a second migration before the first migration is completed
				String timestamp((char *)payloadptr, strlen((char *) payloadptr));
				payloadptr += strlen((char *)payloadptr) + 1;
				signed_datalen = payloadptr - payload;
				click_chatter("ProcessNetworkPacket: MIGRATEACK: timestamp: %s", timestamp.c_str());

				// Get the signature (migrated_DAG, timestamp)
				uint16_t siglen;
				memcpy(&siglen, payloadptr, sizeof(uint16_t));
				click_chatter("ProcessNetworkPacket: MIGRATEACK: siglen: %d", siglen);
				payloadptr += sizeof(uint16_t);
				uint8_t *signature = (uint8_t *) calloc(siglen, 1);
				memcpy(signature, payloadptr, siglen);
				payloadptr += siglen;

				// Get the Public key of the fixed host
				uint16_t pubkeylen;
				memcpy(&pubkeylen, payloadptr, sizeof(uint16_t));
				click_chatter("ProcessNetworkPacket: MIGRATEACK: pubkeylen: %d", pubkeylen);
				payloadptr += sizeof(uint16_t);
				char *pubkey = (char *) calloc(pubkeylen, 1);
				memcpy(pubkey, payloadptr, pubkeylen);
				click_chatter("ProcessNetworkPacket: MIGRATEACK: pubkey:%s:", pubkey);
				payloadptr += pubkeylen;
				if(payloadptr-payload != payload_len) {
					click_chatter("ProcessNetworkPacket: WARNING: MIGRATEACK expected payload len=%d, got %d", payload_len, payloadptr-payload);
				}
				//assert(payloadptr-payload == payload_len);

				// 2. Verify hash of pubkey matches the fixed host's SID
				String fixed_SID_string = daginfo->dst_path.xid(daginfo->dst_path.destination_node()).unparse();
				uint8_t pubkeyhash[SHA_DIGEST_LENGTH];
				char pubkeyhash_hexdigest[XIA_SHA_DIGEST_STR_LEN];
				xs_getPubkeyHash(pubkey, pubkeyhash, sizeof pubkeyhash);
				xs_hexDigest(pubkeyhash, sizeof pubkeyhash, pubkeyhash_hexdigest, sizeof pubkeyhash_hexdigest);
				if(strcmp(pubkeyhash_hexdigest, xs_XIDHash(fixed_SID_string.c_str())) != 0) {
					click_chatter("ProcessNetworkPacket: ERROR: MIGRATEACK: Mismatch: fixedSID: %s, pubkeyhash: %s", fixed_SID_string.c_str(), pubkeyhash_hexdigest);
				}
				click_chatter("ProcessNetworkPacket: Hash of pubkey matches fixed SID");

				// 3. Verify Signature using Pubkey
				if(!xs_isValidSignature(payload, signed_datalen, signature, siglen, pubkey, pubkeylen)) {
					click_chatter("ProcessNetworkPacket: ERROR: MIGRATEACK: MIGRATE with invalid signature");
				}
				click_chatter("ProcessNetworkPacket: MIGRATEACK: Signature verified");
				free(signature);
				free(pubkey);

				// 4. Verify timestamp matches the latest migrate message
				if(strcmp(daginfo->last_migrate_ts.c_str(), timestamp.c_str()) != 0) {
					click_chatter("ProcessNetworkPacket: WARN: timestamp sent:%s:, migrateack has:%s:", daginfo->last_migrate_ts.c_str(), timestamp.c_str());
				}
				click_chatter("ProcessNetworkPacket: MIGRATEACK: verified timestamp");

				// 5. Update DAGinfo src_path to use the new DAG
				// TODO: Verify migrated_DAG's destination node is the same as src_path's
				//       before replacing with the migrated_DAG
				daginfo->src_path.parse(migrated_DAG);
				click_chatter("ProcessNetworkPacket: MIGRATEACK: updated daginfo with newly acknowledged DAG");

				// 6. The data retransmissions can now resume
				daginfo->migrateack_waiting = false;
				daginfo->num_migrate_tries = 0;

				bool resetTimer = false;

				portToDAGinfo.set(_dport, *daginfo);

			} else {
				//printf("port not found\n");
			}

		} else if (thdr.pkt_info() == TransportHeader::DATA) {
			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			// Get the dst port from XIDpair table
			_dport = XIDpairToPort.get(xid_pair);

			//printf("(%s) my_sport=%d  my_sid=%s  his_sid=%s\n", (_local_addr.unparse()).c_str(),  _dport,  _destination_xid.unparse().c_str(), _source_xid.unparse().c_str());
			HashTable<unsigned short, bool>::iterator it1;
			it1 = portToActive.find(_dport);

			if(it1 != portToActive.end() ) {

				DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);

				if (thdr.seq_num() == daginfo->expected_seqnum) {
					daginfo->expected_seqnum++;
					//printf("(%s) Accept Received data (now expected seq=%d)\n", (_local_addr.unparse()).c_str(), daginfo->expected_seqnum);
				} else {
					sendToApplication = false;
					printf("expected sequence # %d, received %d\n", daginfo->expected_seqnum, thdr.seq_num());
					printf("(%s) Discarded Received data\n", (_local_addr.unparse()).c_str());
				}

				portToDAGinfo.set(_dport, *daginfo);
			
				//In case of Client Mobility...	 Update 'daginfo->dst_path'
				//daginfo->dst_path = src_path;

				// send the cumulative ACK to the sender
				//Add XIA headers
				XIAHeaderEncap xiah_new;
				xiah_new.set_nxt(CLICK_XIA_NXT_TRN);
				xiah_new.set_last(LAST_NODE_DEFAULT);
				xiah_new.set_hlim(HLIM_DEFAULT);
				xiah_new.set_dst_path(src_path);
				xiah_new.set_src_path(dst_path);

				const char* dummy = "cumulative_ACK";
				WritablePacket *just_payload_part = WritablePacket::make(256, dummy, strlen(dummy), 0);

				WritablePacket *p = NULL;

				xiah_new.set_plen(strlen(dummy));
				//click_chatter("Sent packet to network");

				TransportHeaderEncap *thdr_new = TransportHeaderEncap::MakeACKHeader( 0, daginfo->expected_seqnum, 0); // #seq, #ack, length
				p = thdr_new->encap(just_payload_part);

				thdr_new->update();
				xiah_new.set_plen(strlen(dummy) + thdr_new->hlen()); // XIA payload = transport header + transport-layer data

				p = xiah_new.encap(p, false);
				delete thdr_new;

				XIAHeader xiah1(p);
				String pld((char *)xiah1.payload(), xiah1.plen());
				//printf("\n\n (%s) send=%s  len=%d \n\n", (_local_addr.unparse()).c_str(), pld.c_str(), xiah1.plen());

				output(NETWORK_PORT).push(p);

			} else {
				printf("destination port not found: %d\n", _dport);
				sendToApplication = false;
			}

		} else if (thdr.pkt_info() == TransportHeader::ACK) {
			sendToApplication = false;

			XIDpair xid_pair;
			xid_pair.set_src(_destination_xid);
			xid_pair.set_dst(_source_xid);

			// Get the dst port from XIDpair table
			_dport = XIDpairToPort.get(xid_pair);

			HashTable<unsigned short, bool>::iterator it1;
			it1 = portToActive.find(_dport);

			if(it1 != portToActive.end() ) {
				DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);
			
				//In case of Client Mobility...	 Update 'daginfo->dst_path'
				//daginfo->dst_path = src_path;

				int expected_seqnum = thdr.ack_num();

				bool resetTimer = false;

				// Clear all Acked packets
				for (int i = daginfo->base; i < expected_seqnum; i++) {
					int idx = i % MAX_WIN_SIZE;
					if (daginfo->sent_pkt[idx]) {
						daginfo->sent_pkt[idx]->kill();
						daginfo->sent_pkt[idx] = NULL;
					}
				
					resetTimer = true;
				}

				// Update the variables
				daginfo->base = expected_seqnum;

				// Reset timer
				if (resetTimer) {
					daginfo->timer_on = true;
					daginfo->dataack_waiting = true;
					// FIXME: should we reset retransmit_tries here?
					daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);

					if (! _timer.scheduled() || _timer.expiry() >= daginfo->expiry )
						_timer.reschedule_at(daginfo->expiry);

					if (daginfo->base == daginfo->next_seqnum) {

						// Clear timer
						daginfo->timer_on = false;
						daginfo->dataack_waiting = false;
						daginfo->num_retransmit_tries = 0;
						//daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);
					}
				}

				portToDAGinfo.set(_dport, *daginfo);

			} else {
				//printf("port not found\n");
			}

		} else if (thdr.pkt_info() == TransportHeader::FIN) {
			//printf("FIN received, doing nothing\n");
		}
		else {
			printf("UNKNOWN dport = %d send = %d hdr=%d\n", _dport, sendToApplication, thdr.pkt_info());		
		}

	} else if (thdr.type() == TransportHeader::XSOCK_DGRAM) {

		_dport = XIDtoPort.get(_destination_xid);
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);
		// check if _destination_sid is of XSOCK_DGRAM
		if (daginfo->sock_type != SOCK_DGRAM) {
			click_chatter("DGRAMERROR: socket type:%d: expected:%d:, Delivering to application Anyway", daginfo->sock_type, SOCK_DGRAM);
			//sendToApplication = false;
		}
	
	} else {
		printf("UNKNOWN!!!!! dport = %d\n", _dport);
	}


	if(_dport && sendToApplication) {
		//TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
		DAGinfo daginfo = portToDAGinfo.get(_dport);

		if(daginfo.initialized == false) {
			daginfo.dst_path = xiah.src_path();
			daginfo.initialized = true;
			portToDAGinfo.set(_dport, daginfo);
		}

		// FIXME: what is this? need constant here
		if(xiah.nxt() == 22 && daginfo.isConnected == true)
		{
			//Verify mobility info
			daginfo.dst_path = xiah.src_path();
			portToDAGinfo.set(_dport, daginfo);
			click_chatter("###############################################################################################################################################################################3############################3Sender moved, update to the new DAG");

		} else {
			//Unparse dag info
			String src_path = xiah.src_path().unparse();
			String payload((const char*)thdr.payload(), xiah.plen() - thdr.hlen());

			xia::XSocketMsg xsm;
			xsm.set_type(xia::XRECV);
			xia::X_Recv_Msg *x_recv_msg = xsm.mutable_x_recv();
			x_recv_msg->set_dag(src_path.c_str());
			x_recv_msg->set_payload(payload.c_str(), payload.length());

			std::string p_buf;
			xsm.SerializeToString(&p_buf);

			WritablePacket *p2 = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

			if (DEBUG)
				click_chatter("Sent packet to socket with port %d", _dport);
			output(API_PORT).push(UDPIPPrep(p2, _dport));
		}

	} else {
		if (!_dport) {
			click_chatter("Packet to unknown port %d XID=%s, sendToApp=%d", _dport, _destination_xid.unparse().c_str(), sendToApplication );
		}
	}
}

void XTRANSPORT::ProcessCachePacket(WritablePacket *p_in)
{
// 	if (DEBUG){
// 	click_chatter("Got packet from cache");		
//  	}
	
	//Extract the SID/CID
	XIAHeader xiah(p_in->xia_header());
	XIAPath dst_path = xiah.dst_path();
	XIAPath src_path = xiah.src_path();
	XID	destination_sid = dst_path.xid(dst_path.destination_node());
	XID	source_cid = src_path.xid(src_path.destination_node());	
	
        ContentHeader ch(p_in);
	
// 	click_chatter("dest %s, src_cid %s, dst_path: %s, src_path: %s\n", 
// 		      destination_sid.unparse().c_str(), source_cid.unparse().c_str(), dst_path.unparse().c_str(), src_path.unparse().c_str());
// 	click_chatter("dst_path: %s, src_path: %s, OPCode: %d\n", dst_path.unparse().c_str(), src_path.unparse().c_str(), ch.opcode());
	
	
        if(ch.opcode()==ContentHeader::OP_PUSH){
		// compute the hash and verify it matches the CID
		String hash = "CID:";
		char hexBuf[3];
		int i = 0;
		SHA1_ctx sha_ctx;
		unsigned char digest[HASH_KEYSIZE];
		SHA1_init(&sha_ctx);
		SHA1_update(&sha_ctx, (unsigned char *)xiah.payload(), xiah.plen());
		SHA1_final(digest, &sha_ctx);
		for(i = 0; i < HASH_KEYSIZE; i++) {
			sprintf(hexBuf, "%02x", digest[i]);
			hash.append(const_cast<char *>(hexBuf), 2);
		}

// 		int status = READY_TO_READ;
		if (hash != source_cid.unparse()) {
			click_chatter("CID with invalid hash received: %s\n", source_cid.unparse().c_str());
// 			status = INVALID_HASH;
		}
		
		unsigned short _dport = XIDtoPushPort.get(destination_sid);
		if(!_dport){
			click_chatter("Couldn't find SID to send to: %s\n", destination_sid.unparse().c_str());
			return;
		}
		
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);
		// check if _destination_sid is of XSOCK_DGRAM
		if (daginfo->sock_type != XSOCKET_CHUNK) {
			click_chatter("This is not a chunk socket. dport: %i, Socktype: %i", _dport, daginfo->sock_type);
		}
		
		// Send pkt up

		//Unparse dag info
		String src_path = xiah.src_path().unparse();

		xia::XSocketMsg xia_socket_msg;
		xia_socket_msg.set_type(xia::XPUSHCHUNKTO);
		xia::X_Pushchunkto_Msg *x_pushchunkto_msg = xia_socket_msg.mutable_x_pushchunkto();
		x_pushchunkto_msg->set_cid(source_cid.unparse().c_str());
		x_pushchunkto_msg->set_payload((const char*)xiah.payload(), xiah.plen());
		x_pushchunkto_msg->set_cachepolicy(ch.cachePolicy());
		x_pushchunkto_msg->set_ttl(ch.ttl());
		x_pushchunkto_msg->set_cachesize(ch.cacheSize());
		x_pushchunkto_msg->set_contextid(ch.contextID());
		x_pushchunkto_msg->set_length(ch.length());
 		x_pushchunkto_msg->set_ddag(dst_path.unparse().c_str());

		std::string p_buf;
		xia_socket_msg.SerializeToString(&p_buf);

		WritablePacket *p2 = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

		//printf("FROM CACHE. data length = %d  \n", str.length());
 		if (DEBUG)
		click_chatter("Sent packet to socket: sport %d dport %d", _dport, _dport);

		output(API_PORT).push(UDPIPPrep(p2, _dport));
		return;
		
		
	}

	XIDpair xid_pair;
	xid_pair.set_src(destination_sid);
	xid_pair.set_dst(source_cid);

	unsigned short _dport = XIDpairToPort.get(xid_pair);
	
// 	click_chatter(">>packet from processCACHEpackets %d\n", _dport);
// 	click_chatter("CachePacket, Src: %s, Dest: %s, Local: %s", xiah.dst_path().unparse().c_str(),
// 			      xiah.src_path().unparse().c_str(), _local_addr.unparse_re().c_str());
	click_chatter("CachePacket, dest: %s, src_cid %s OPCode: %d \n", destination_sid.unparse().c_str(), source_cid.unparse().c_str(), ch.opcode());
// 	click_chatter("dst_path: %s, src_path: %s, OPCode: %d\n", dst_path.unparse().c_str(), src_path.unparse().c_str(), ch.opcode());

	if(_dport)
	{
		//TODO: Refine the way we change DAG in case of migration. Use some control bits. Add verification
		//DAGinfo daginfo=portToDAGinfo.get(_dport);
		//daginfo.dst_path=xiah.src_path();
		//portToDAGinfo.set(_dport,daginfo);
		//ENDTODO

		DAGinfo *daginfo = portToDAGinfo.get_pointer(_dport);

		// Reset timer or just Remove the corresponding entry in the hash tables (Done below)
		HashTable<XID, WritablePacket*>::iterator it1;
		it1 = daginfo->XIDtoCIDreqPkt.find(source_cid);

		if(it1 != daginfo->XIDtoCIDreqPkt.end() ) {
			// Remove the entry
			daginfo->XIDtoCIDreqPkt.erase(it1);
		}

		HashTable<XID, Timestamp>::iterator it2;
		it2 = daginfo->XIDtoExpiryTime.find(source_cid);

		if(it2 != daginfo->XIDtoExpiryTime.end()) {
			// Remove the entry
			daginfo->XIDtoExpiryTime.erase(it2);
		}

		HashTable<XID, bool>::iterator it3;
		it3 = daginfo->XIDtoTimerOn.find(source_cid);

		if(it3 != daginfo->XIDtoTimerOn.end()) {
			// Remove the entry
			daginfo->XIDtoTimerOn.erase(it3);
		}

		// compute the hash and verify it matches the CID
		String hash = "CID:";
		char hexBuf[3];
		int i = 0;
		SHA1_ctx sha_ctx;
		unsigned char digest[HASH_KEYSIZE];
		SHA1_init(&sha_ctx);
		SHA1_update(&sha_ctx, (unsigned char *)xiah.payload(), xiah.plen());
		SHA1_final(digest, &sha_ctx);
		for(i = 0; i < HASH_KEYSIZE; i++) {
			sprintf(hexBuf, "%02x", digest[i]);
			hash.append(const_cast<char *>(hexBuf), 2);
		}

		int status = READY_TO_READ;
		if (hash != source_cid.unparse()) {
			click_chatter("CID with invalid hash received: %s\n", source_cid.unparse().c_str());
			status = INVALID_HASH;
		}

		// Update the status of CID request
		daginfo->XIDtoStatus.set(source_cid, status);

		// Check if the ReadCID() was called for this CID
		HashTable<XID, bool>::iterator it4;
		it4 = daginfo->XIDtoReadReq.find(source_cid);

		if(it4 != daginfo->XIDtoReadReq.end()) {
			// There is an entry
			bool read_cid_req = it4->second;

			if (read_cid_req == true) {
				// Send pkt up
				daginfo->XIDtoReadReq.erase(it4);

				portToDAGinfo.set(_dport, *daginfo);

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

				//printf("FROM CACHE. data length = %d  \n", str.length());
				if (DEBUG)
					click_chatter("Sent packet to socket: sport %d dport %d \n", _dport, _dport);

				output(API_PORT).push(UDPIPPrep(p2, _dport));

			} else {
				// Store the packet into temp buffer (until ReadCID() is called for this CID)
				WritablePacket *copy_response_pkt = copy_cid_response_packet(p_in, daginfo);
				daginfo->XIDtoCIDresponsePkt.set(source_cid, copy_response_pkt);

				portToDAGinfo.set(_dport, *daginfo);
			}

		} else {
			WritablePacket *copy_response_pkt = copy_cid_response_packet(p_in, daginfo);
			daginfo->XIDtoCIDresponsePkt.set(source_cid, copy_response_pkt);
			portToDAGinfo.set(_dport, *daginfo);
		}
	}
	else
	{
		click_chatter("Case 2. Packet to unknown %s, src_cid %s\n", destination_sid.unparse().c_str(), source_cid.unparse().c_str());
// 		click_chatter("Case 2. Packet to unknown dest %s, src_cid %s, dst_path: %s, src_path: %s\n", 
// 		      destination_sid.unparse().c_str(), source_cid.unparse().c_str(), dst_path.unparse().c_str(), src_path.unparse().c_str());
// 		click_chatter("Case 2. Packet to unknown  dst_path: %s, src_path: %s\n", dst_path.unparse().c_str(), src_path.unparse().c_str());

	  
	}
	
	
	
	

	
}

void XTRANSPORT::ProcessXhcpPacket(WritablePacket *p_in)
{
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


void XTRANSPORT::push(int port, Packet *p_input)
{
//	pthread_mutex_lock(&_lock);

	WritablePacket *p_in = p_input->uniqueify();
	//Depending on which CLICK-module-port it arrives at it could be control/API traffic/Data traffic

	switch(port) { // This is a "CLICK" port of UDP module.
	case API_PORT:	// control packet from socket API
		ProcessAPIPacket(p_in);
		break;

	case BAD_PORT: //packet from ???
        if (DEBUG)
            click_chatter("\n\nERROR: BAD INPUT PORT TO XTRANSPORT!!!\n\n");
		break;

	case NETWORK_PORT: //Packet from network layer
		ProcessNetworkPacket(p_in);
		p_in->kill();
		break;

	case CACHE_PORT:	//Packet from cache
//  		click_chatter(">>>>> Cache Port packet from socket Cache %d\n", port);
		ProcessCachePacket(p_in);
		p_in->kill();
		break;

	case XHCP_PORT:		//Packet with DHCP information
		ProcessXhcpPacket(p_in);
		p_in->kill();
		break;

	default:
		click_chatter("packet from unknown port: %d\n", port);
		break;
	}

//	pthread_mutex_unlock(&_lock);
}

void XTRANSPORT::ReturnResult(int sport, xia::XSocketCallType type, int rc, int err)
{
//	click_chatter("sport=%d type=%d rc=%d err=%d\n", sport, type, rc, err);
	xia::XSocketMsg xia_socket_msg_response;
	xia_socket_msg_response.set_type(xia::XRESULT);
	xia::X_Result_Msg *x_result = xia_socket_msg_response.mutable_x_result();
	x_result->set_type(type);
	x_result->set_return_code(rc);
	x_result->set_err_code(err);

	std::string p_buf;
	xia_socket_msg_response.SerializeToString(&p_buf);
	WritablePacket *reply = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, sport));
}

Packet *
XTRANSPORT::UDPIPPrep(Packet *p_in, int dport)
{
    p_in->set_dst_ip_anno(IPAddress("127.0.0.1"));
    SET_DST_PORT_ANNO(p_in, dport);

	return p_in;
}


enum {H_MOVE};

int XTRANSPORT::write_param(const String &conf, Element *e, void *vparam,
							ErrorHandler *errh)
{
	XTRANSPORT *f = static_cast<XTRANSPORT *>(e);
	switch(reinterpret_cast<intptr_t>(vparam)) {
	case H_MOVE:
	{
		XIAPath local_addr;
		if (cp_va_kparse(conf, f, errh,
						 "LOCAL_ADDR", cpkP + cpkM, cpXIAPath, &local_addr,
						 cpEnd) < 0)
			return -1;
		f->_local_addr = local_addr;
		click_chatter("Moved to %s", local_addr.unparse().c_str());
		f->_local_hid = local_addr.xid(local_addr.destination_node());

	}
	break;
	default:
		break;
	}
	return 0;
}

void XTRANSPORT::add_handlers() {
	add_write_handler("local_addr", write_param, (void *)H_MOVE);
}

/*
** Handler for the Xsocket API call
**
** FIXME: why is xia_socket_msg part of the xtransport class and not a local variable?????
*/
void XTRANSPORT::Xsocket(unsigned short _sport) {
	//Open socket.
	//click_chatter("Xsocket: create socket %d\n", _sport);

	xia::X_Socket_Msg *x_socket_msg = xia_socket_msg.mutable_x_socket();
	int sock_type = x_socket_msg->type();

	//Set the source port in DAGinfo
	DAGinfo daginfo;
	daginfo.port = _sport;
	daginfo.timer_on = false;
	daginfo.synack_waiting = false;
	daginfo.dataack_waiting = false;
	daginfo.migrateack_waiting = false;
	daginfo.num_retransmit_tries = 0;
	daginfo.teardown_waiting = false;
	daginfo.isConnected = false;
	daginfo.isAcceptSocket = false;
	daginfo.num_connect_tries = 0; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)
	daginfo.num_migrate_tries = 0; // number of migrate tries (Connection will fail after MAX_MIGRATE_TRIES trials)
	memset(daginfo.sent_pkt, 0, MAX_WIN_SIZE * sizeof(WritablePacket*));

	//Set the socket_type (reliable or not) in DAGinfo
	daginfo.sock_type = sock_type;

	// Map the source port to DagInfo
	portToDAGinfo.set(_sport, daginfo);

	portToActive.set(_sport, true);

	hlim.set(_sport, HLIM_DEFAULT);
	nxt_xport.set(_sport, CLICK_XIA_NXT_TRN);

	// printf("XSOCKET: sport=%hu\n", _sport);

	// (for Ack purpose) Reply with a packet with the destination port=source port
	ReturnResult(_sport, xia::XSOCKET, 0);
	// output(API_PORT).push(UDPIPPrep(p_in,_sport));
}

/*
** Xsetsockopt API handler
*/
void XTRANSPORT::Xsetsockopt(unsigned short _sport) {

	// click_chatter("\nSet Socket Option\n");
	xia::X_Setsockopt_Msg *x_sso_msg = xia_socket_msg.mutable_x_setsockopt();

	switch (x_sso_msg->opt_type())
	{
		// FIXME: need real opt type for protobufs
	case 1:
	{
		int hl = x_sso_msg->int_opt();
	
		hlim.set(_sport, hl);
		//click_chatter("sso:hlim:%d\n",hl);
	}
	break;

	case 2:
	{
		int nxt = x_sso_msg->int_opt();
		nxt_xport.set(_sport, nxt);
		if (nxt == CLICK_XIA_NXT_XCMP)
			xcmp_listeners.push_back(_sport);
		else
			xcmp_listeners.remove(_sport);
	}
	break;

	default:
		// unsupported option
		break;
	}
	ReturnResult(_sport, xia::XSETSOCKOPT);
}

/*
** Xgetsockopt API handler
*/
void XTRANSPORT::Xgetsockopt(unsigned short _sport) {
	// click_chatter("\nGet Socket Option\n");
	xia::X_Getsockopt_Msg *x_sso_msg = xia_socket_msg.mutable_x_getsockopt();

	// click_chatter("opt = %d\n", x_sso_msg->opt_type());
	switch (x_sso_msg->opt_type())
	{
	// FIXME: need real opt type for protobufs
	case 1:
	{
		x_sso_msg->set_int_opt(hlim.get(_sport));
		//click_chatter("gso:hlim:%d\n", hlim.get(_sport));
	}
	break;

	case 2:
	{
		x_sso_msg->set_int_opt(nxt_xport.get(_sport));
	}
	break;

	default:
		// unsupported option
		break;
	}
	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	WritablePacket *reply = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::Xbind(unsigned short _sport) {

	int rc = 0, ec = 0;

	//Bind XID
	//click_chatter("\n\nOK: SOCKET BIND !!!\\n");
	//get source DAG from protobuf message

	xia::X_Bind_Msg *x_bind_msg = xia_socket_msg.mutable_x_bind();

	String sdag_string(x_bind_msg->sdag().c_str(), x_bind_msg->sdag().size());

	//String sdag_string((const char*)p_in->data(),(const char*)p_in->end_data());
//	if (DEBUG)
//		click_chatter("\nbind requested to %s, length=%d\n", sdag_string.c_str(), (int)p_in->length());

	//String str_local_addr=_local_addr.unparse();
	//str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID

	//Set the source DAG in DAGinfo
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);
	if (daginfo->src_path.parse(sdag_string)) {
		daginfo->nxt = LAST_NODE_DEFAULT;
		daginfo->last = LAST_NODE_DEFAULT;
		daginfo->hlim = hlim.get(_sport);
		daginfo->isConnected = false;
		daginfo->initialized = true;

		//Check if binding to full DAG or just to SID only
		Vector<XIAPath::handle_t> xids = daginfo->src_path.next_nodes( daginfo->src_path.source_node() );		
		XID front_xid = daginfo->src_path.xid( xids[0] );
		struct click_xia_xid head_xid = front_xid.xid();
		uint32_t head_xid_type = head_xid.type;
		if(head_xid_type == _sid_type) {
			daginfo->full_src_dag = false; 
		} else {
			daginfo->full_src_dag = true;
		}

		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		//XID xid(xid_string);
		//TODO: Add a check to see if XID is already being used

		// Map the source XID to source port (for now, for either type of tranports)
		XIDtoPort.set(source_xid, _sport);
		addRoute(source_xid);

		portToDAGinfo.set(_sport, *daginfo);

		//click_chatter("Bound");
		//click_chatter("set %d %d",_sport, __LINE__);

	} else {
		rc = -1;
		ec = EADDRNOTAVAIL;
	}

	// (for Ack purpose) Reply with a packet with the destination port=source port
	ReturnResult(_sport, xia::XBIND, rc, ec);
}

// FIXME: This way of doing things is a bit hacky.
void XTRANSPORT::XbindPush(unsigned short _sport) {

	int rc = 0, ec = 0;

	//Bind XID
// 	click_chatter("\n\nOK: SOCKET BIND !!!\\n");
	//get source DAG from protobuf message

	xia::X_BindPush_Msg *x_bindpush_msg = xia_socket_msg.mutable_x_bindpush();

	String sdag_string(x_bindpush_msg->sdag().c_str(), x_bindpush_msg->sdag().size());

	//String sdag_string((const char*)p_in->data(),(const char*)p_in->end_data());
//	if (DEBUG)
//		click_chatter("\nbind requested to %s, length=%d\n", sdag_string.c_str(), (int)p_in->length());

	//String str_local_addr=_local_addr.unparse();
	//str_local_addr=str_local_addr+" "+xid_string;//Make source DAG _local_addr:SID

	//Set the source DAG in DAGinfo
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);
	if (daginfo->src_path.parse(sdag_string)) {
		daginfo->nxt = LAST_NODE_DEFAULT;
		daginfo->last = LAST_NODE_DEFAULT;
		daginfo->hlim = hlim.get(_sport);
		daginfo->isConnected = false;
		daginfo->initialized = true;

		//Check if binding to full DAG or just to SID only
		Vector<XIAPath::handle_t> xids = daginfo->src_path.next_nodes( daginfo->src_path.source_node() );		
		XID front_xid = daginfo->src_path.xid( xids[0] );
		struct click_xia_xid head_xid = front_xid.xid();
		uint32_t head_xid_type = head_xid.type;
		if(head_xid_type == _sid_type) {
			daginfo->full_src_dag = false; 
		} else {
			daginfo->full_src_dag = true;
		}

		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		//XID xid(xid_string);
		//TODO: Add a check to see if XID is already being used

		// Map the source XID to source port (for now, for either type of tranports)
		XIDtoPushPort.set(source_xid, _sport);
		addRoute(source_xid);

		portToDAGinfo.set(_sport, *daginfo);

		//click_chatter("Bound");
		//click_chatter("set %d %d",_sport, __LINE__);

	} else {
		rc = -1;
		ec = EADDRNOTAVAIL;
		click_chatter("\n\nERROR: SOCKET PUSH BIND !!!\\n");
	}
	
	// (for Ack purpose) Reply with a packet with the destination port=source port
// 	click_chatter("\n\nPUSHBIND: DONE, SENDING ACK !!!\\n");
	ReturnResult(_sport, xia::XBINDPUSH, rc, ec);
// 	click_chatter("\n\nAFTER PUSHBIND: DONE, SENDING ACK !!!\\n");
}

void XTRANSPORT::Xclose(unsigned short _sport)
{
	// Close port
	//click_chatter("Xclose: closing %d\n", _sport);

	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	// Set timer
	daginfo->timer_on = true;
	daginfo->teardown_waiting = true;
	daginfo->teardown_expiry = Timestamp::now() + Timestamp::make_msec(_teardown_wait_ms);

	if (! _timer.scheduled() || _timer.expiry() >= daginfo->teardown_expiry )
		_timer.reschedule_at(daginfo->teardown_expiry);

	portToDAGinfo.set(_sport, *daginfo);

	xcmp_listeners.remove(_sport);

	// (for Ack purpose) Reply with a packet with the destination port=source port
	ReturnResult(_sport, xia::XCLOSE);
}

void XTRANSPORT::Xconnect(unsigned short _sport)
{
	//click_chatter("Xconect: connecting %d\n", _sport);

	//isConnected=true;
	//String dest((const char*)p_in->data(),(const char*)p_in->end_data());
	//click_chatter("\nconnect to %s, length=%d\n",dest.c_str(),(int)p_in->length());

	xia::X_Connect_Msg *x_connect_msg = xia_socket_msg.mutable_x_connect();

	String dest(x_connect_msg->ddag().c_str());

	//String sdag_string((const char*)p_in->data(),(const char*)p_in->end_data());
	//click_chatter("\nconnect requested to %s, length=%d\n",dest.c_str(),(int)p_in->length());

	XIAPath dst_path;
	dst_path.parse(dest);

	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);
	//click_chatter("connect %d %x",_sport, daginfo);

	if(!daginfo) {
		//click_chatter("Create DAGINFO connect %d %x",_sport, daginfo);
		//No local SID bound yet, so bind ephemeral one
		daginfo = new DAGinfo();
	}

	daginfo->dst_path = dst_path;
	daginfo->port = _sport;
	daginfo->isConnected = true;
	daginfo->initialized = true;
	daginfo->ack_num = 0;
	daginfo->base = 0;
	daginfo->next_seqnum = 0;
	daginfo->expected_seqnum = 0;
	daginfo->num_connect_tries++; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)

	String str_local_addr = _local_addr.unparse_re();
	//String dagstr = daginfo->src_path.unparse_re();

	// API sends a temporary DAG, if permanent not assigned by bind
	if(x_connect_msg->has_sdag()) {
		String sdag_string(x_connect_msg->sdag().c_str(), x_connect_msg->sdag().size());
		daginfo->src_path.parse(sdag_string);
	}
	// src_path must be set by Xbind() or Xconnect() API
	assert(daginfo->src_path.is_valid());

	daginfo->nxt = LAST_NODE_DEFAULT;
	daginfo->last = LAST_NODE_DEFAULT;
	daginfo->hlim = hlim.get(_sport);

	XID source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
	XID destination_xid = daginfo->dst_path.xid(daginfo->dst_path.destination_node());

	XIDpair xid_pair;
	xid_pair.set_src(source_xid);
	xid_pair.set_dst(destination_xid);

	// Map the src & dst XID pair to source port
	//printf("setting pair to port1 %d\n", _sport);

	XIDpairToPort.set(xid_pair, _sport);

	// Map the source XID to source port
	XIDtoPort.set(source_xid, _sport);
	addRoute(source_xid);

	// click_chatter("XCONNECT: set %d %x",_sport, daginfo);

	// Prepare SYN packet

	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_nxt(CLICK_XIA_NXT_TRN);
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(hlim.get(_sport));
	xiah.set_dst_path(dst_path);
	xiah.set_src_path(daginfo->src_path);

	//click_chatter("Sent packet to network");
	const char* dummy = "Connection_request";
	WritablePacket *just_payload_part = WritablePacket::make(256, dummy, strlen(dummy), 20);

	WritablePacket *p = NULL;

	TransportHeaderEncap *thdr = TransportHeaderEncap::MakeSYNHeader( 0, -1, 0); // #seq, #ack, length

	p = thdr->encap(just_payload_part);

	thdr->update();
	xiah.set_plen(strlen(dummy) + thdr->hlen()); // XIA payload = transport header + transport-layer data

	p = xiah.encap(p, false);

	delete thdr;

	// Set timer
	daginfo->timer_on = true;
	daginfo->synack_waiting = true;
	daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);

	if (! _timer.scheduled() || _timer.expiry() >= daginfo->expiry )
		_timer.reschedule_at(daginfo->expiry);

	// Store the syn packet for potential retransmission
	daginfo->syn_pkt = copy_packet(p, daginfo);

	portToDAGinfo.set(_sport, *daginfo);
	XIAHeader xiah1(p);
	//String pld((char *)xiah1.payload(), xiah1.plen());
	// printf("XCONNECT: %d: %s\n", _sport, (_local_addr.unparse()).c_str());
	output(NETWORK_PORT).push(p);

	//daginfo=portToDAGinfo.get_pointer(_sport);
	//click_chatter("\nbound to %s\n",portToDAGinfo.get_pointer(_sport)->src_path.unparse().c_str());

	// (for Ack purpose) Reply with a packet with the destination port=source port
	//output(API_PORT).push(UDPIPPrep(p_in,_sport));
}

void XTRANSPORT::Xaccept(unsigned short _sport)
{
	//click_chatter("Xaccept: on %d\n", _sport);
	hlim.set(_sport, HLIM_DEFAULT);
	nxt_xport.set(_sport, CLICK_XIA_NXT_TRN);

	if (!pending_connection_buf.empty()) {

		DAGinfo daginfo = pending_connection_buf.front();
		daginfo.port = _sport;

		daginfo.ack_num = 0;
		daginfo.base = 0;
		daginfo.hlim = hlim.get(_sport);
		daginfo.next_seqnum = 0;
		daginfo.expected_seqnum = 0;
		daginfo.isAcceptSocket = true;
		memset(daginfo.sent_pkt, 0, MAX_WIN_SIZE * sizeof(WritablePacket*));

		portToDAGinfo.set(_sport, daginfo);

		click_chatter("Xaccept: Daginfo.src_path:%s", daginfo.src_path.unparse().c_str());
		click_chatter("Xaccept: Daginfo.dst_path:%s", daginfo.dst_path.unparse().c_str());
		XID source_xid = daginfo.src_path.xid(daginfo.src_path.destination_node());
		XID destination_xid = daginfo.dst_path.xid(daginfo.dst_path.destination_node());

		XIDpair xid_pair;
		xid_pair.set_src(source_xid);
		xid_pair.set_dst(destination_xid);

		// Map the src & dst XID pair to source port
		XIDpairToPort.set(xid_pair, _sport);

		portToActive.set(_sport, true);

		// printf("XACCEPT: (%s) my_sport=%d  my_sid=%s  his_sid=%s \n\n", (_local_addr.unparse()).c_str(), _sport, source_xid.unparse().c_str(), destination_xid.unparse().c_str());

		pending_connection_buf.pop();


 		xia::XSocketMsg xsm;
 		xsm.set_type(xia::XACCEPT);

		xia::X_Accept_Msg *msg = xsm.mutable_x_accept();
		msg->set_dag(daginfo.dst_path.unparse().c_str());

 		std::string s;
 		xsm.SerializeToString(&s);
		WritablePacket *reply = WritablePacket::make(256, s.c_str(), s.size(), 0);
		output(API_PORT).push(UDPIPPrep(reply, _sport));
		
//		ReturnResult(_sport, xia::XACCEPT);

	} else {
		// FIXME: what error code should be returned?
		ReturnResult(_sport, xia::XACCEPT, -1, ECONNABORTED);
		click_chatter("\n Xaccept: error\n");
	}
}

void XTRANSPORT::Xchangead(unsigned short _sport)
{
	UNUSED(_sport);

	// Save the old AD
	String str_local_addr = _local_addr.unparse();
	size_t old_AD_start = str_local_addr.find_left("AD:");
	size_t old_AD_end = str_local_addr.find_left(" ", old_AD_start);
	String old_AD_str = str_local_addr.substring(old_AD_start, old_AD_end - old_AD_start);

	xia::X_Changead_Msg *x_changead_msg = xia_socket_msg.mutable_x_changead();
	//String tmp = _local_addr.unparse();
	//Vector<String> ids;
	//cp_spacevec(tmp, ids);
	String AD_str(x_changead_msg->ad().c_str());
	String HID_str = _local_hid.unparse();
	String IP4ID_str(x_changead_msg->ip4id().c_str());
	_local_4id.parse(IP4ID_str);
	String new_local_addr;
	// If a valid 4ID is given, it is included (as a fallback) in the local_addr
	if(_local_4id != _null_4id) {		
		new_local_addr = "RE ( " + IP4ID_str + " ) " + AD_str + " " + HID_str;
	} else {
		new_local_addr = "RE " + AD_str + " " + HID_str;
	}
	click_chatter("new address is - %s", new_local_addr.c_str());
	_local_addr.parse(new_local_addr);		

	// Inform all active stream connections about this change
	for (HashTable<unsigned short, DAGinfo>::iterator iter = portToDAGinfo.begin(); iter != portToDAGinfo.end(); ++iter ) {
		unsigned short _migrateport = iter->first;
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_migrateport);
		// TODO: use XSOCKET_STREAM?
		// Skip non-stream connections
		if(daginfo->sock_type != SOCK_STREAM) {
			continue;
		}
		// Skip inactive ports
		if(daginfo->isConnected == false) {
			click_chatter("Xchangead: skipping migration for non-connected port");
			click_chatter("Xchangead: src_path:%s:", daginfo->src_path.unparse().c_str());
			//click_chatter("Xchangead: dst_path:%s:", daginfo->dst_path.unparse().c_str());
			continue;
		}
		// Update src_path in daginfo
		click_chatter("Xchangead: updating %s to %s in daginfo", old_AD_str.c_str(), AD_str.c_str());
		daginfo->src_path.replace_node_xid(old_AD_str, AD_str);

		// Send MIGRATE message to each corresponding endpoint
		// src_DAG, dst_DAG, timestamp - Signed by private key
		// plus the public key (Should really exchange at SYN/SYNACK)
		uint8_t *payload;
		uint8_t *payloadptr;
		uint32_t maxpayloadlen;
		uint32_t payloadlen;
		String src_path = daginfo->src_path.unparse();
		String dst_path = daginfo->dst_path.unparse();
		click_chatter("Xchangead: MIGRATING %s - %s", src_path.c_str(), dst_path.c_str());
		int src_path_len = strlen(src_path.c_str()) + 1;
		int dst_path_len = strlen(dst_path.c_str()) + 1;
		Timestamp now = Timestamp::now();
		String timestamp = now.unparse();
		int timestamp_len = strlen(timestamp.c_str()) + 1;
		// Get the public key to include in packet
		char pubkey[MAX_PUBKEY_SIZE];
		uint16_t pubkeylen = MAX_PUBKEY_SIZE;
		XID src_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		click_chatter("Xchangead: Retrieving pubkey for xid:%s:", src_xid.unparse().c_str());
		if(xs_getPubkey(src_xid.unparse().c_str(), pubkey, &pubkeylen)) {
			click_chatter("Xchangead: ERROR: Pubkey not found:%s:", src_xid.unparse().c_str());
			return;
		}
		click_chatter("Xchangead: Pubkey:%s:", pubkey);
		maxpayloadlen = src_path_len + dst_path_len + timestamp_len + sizeof(uint16_t) + MAX_SIGNATURE_SIZE + sizeof(uint16_t) + pubkeylen;
		payload = (uint8_t *)calloc(maxpayloadlen, 1);
		if(payload == NULL) {
			click_chatter("Xchangead: ERROR: Cannot allocate memory for Migrate packet");
			return;
		}
		// Build the payload
		payloadptr = payload;
		// Source DAG with new AD
		memcpy(payloadptr, src_path.c_str(), src_path_len);
		click_chatter("Xchangead: MIGRATE Source DAG: %s", payloadptr);
		payloadptr += src_path_len;
		// Destination DAG
		memcpy(payloadptr, dst_path.c_str(), dst_path_len);
		click_chatter("Xchangead: MIGRATE Dest DAG: %s", payloadptr);
		payloadptr += dst_path_len;
		// Timestamp of this MIGRATE message
		memcpy(payloadptr, timestamp.c_str(), timestamp_len);
		click_chatter("Xchangead: MIGRATE Timestamp: %s", timestamp.c_str());
		payloadptr += timestamp_len;
		// Sign(SourceDAG, DestinationDAG, Timestamp)
		uint8_t signature[MAX_SIGNATURE_SIZE];
		uint16_t siglen = MAX_SIGNATURE_SIZE;
		if(xs_sign(src_xid.unparse().c_str(), payload, payloadptr-payload, signature, &siglen)) {
			click_chatter("Xchangead: ERROR: Signing Migrate packet");
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
		click_chatter("Xchangead: MIGRATE: Pubkey:%s: Length: %d", pubkey, pubkeylen);
		payloadptr += pubkeylen;
		// Total payload length
		payloadlen = payloadptr - payload;
		click_chatter("Xchangead: MIGRATE payload length: %d", payloadlen);

		//Add XIA headers
		XIAHeaderEncap xiah;
		xiah.set_nxt(CLICK_XIA_NXT_TRN);
		xiah.set_last(LAST_NODE_DEFAULT);
		xiah.set_hlim(hlim.get(_migrateport));
		xiah.set_dst_path(daginfo->dst_path);
		xiah.set_src_path(daginfo->src_path);

		WritablePacket *just_payload_part = WritablePacket::make(256, payload, payloadlen, 0);
		free(payload);

		WritablePacket *p = NULL;

		TransportHeaderEncap *thdr = TransportHeaderEncap::MakeMIGRATEHeader( 0, -1, 0); // #seq, #ack, length

		p = thdr->encap(just_payload_part);

		thdr->update();
		xiah.set_plen(payloadlen + thdr->hlen()); // XIA payload = transport header + transport-layer data

		p = xiah.encap(p, false);

		delete thdr;

		// Store the migrate packet for potential retransmission
		daginfo->migrate_pkt = copy_packet(p, daginfo);
		daginfo->num_migrate_tries++;
		daginfo->last_migrate_ts = timestamp;

		// Set timer
		daginfo->timer_on = true;
		daginfo->migrateack_waiting = true;
		daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);

		if (! _timer.scheduled() || _timer.expiry() >= daginfo->expiry )
			_timer.reschedule_at(daginfo->expiry);

		portToDAGinfo.set(_migrateport, *daginfo);
		output(NETWORK_PORT).push(p);
	}
}

void XTRANSPORT::Xreadlocalhostaddr(unsigned short _sport)
{
	// read the localhost AD and HID
	String local_addr = _local_addr.unparse();
	size_t AD_found_start = local_addr.find_left("AD:");
	size_t AD_found_end = local_addr.find_left(" ", AD_found_start);
	String AD_str = local_addr.substring(AD_found_start, AD_found_end - AD_found_start);
	String HID_str = _local_hid.unparse();
	String IP4ID_str = _local_4id.unparse();
	// return a packet containing localhost AD and HID
	xia::XSocketMsg _Response;
	_Response.set_type(xia::XREADLOCALHOSTADDR);
	xia::X_ReadLocalHostAddr_Msg *_msg = _Response.mutable_x_readlocalhostaddr();
	_msg->set_ad(AD_str.c_str());
	_msg->set_hid(HID_str.c_str());
	_msg->set_ip4id(IP4ID_str.c_str());
	std::string p_buf1;
	_Response.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::Xupdatenameserverdag(unsigned short _sport)
{
	UNUSED(_sport);

	xia::X_Updatenameserverdag_Msg *x_updatenameserverdag_msg = xia_socket_msg.mutable_x_updatenameserverdag();
	String ns_dag(x_updatenameserverdag_msg->dag().c_str());
	//click_chatter("new nameserver address is - %s", ns_dag.c_str());
	_nameserver_addr.parse(ns_dag);
}

void XTRANSPORT::Xreadnameserverdag(unsigned short _sport)
{
	// read the nameserver DAG
	String ns_addr = _nameserver_addr.unparse();
	// return a packet containing the nameserver DAG
	xia::XSocketMsg _Response;
	_Response.set_type(xia::XREADNAMESERVERDAG);
	xia::X_ReadNameServerDag_Msg *_msg = _Response.mutable_x_readnameserverdag();
	_msg->set_dag(ns_addr.c_str());
	std::string p_buf1;
	_Response.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::Xisdualstackrouter(unsigned short _sport)
{
	// return a packet indicating whether this node is an XIA-IPv4 dual-stack router
	xia::XSocketMsg _Response;
	_Response.set_type(xia::XISDUALSTACKROUTER);
	xia::X_IsDualStackRouter_Msg *_msg = _Response.mutable_x_isdualstackrouter();
	_msg->set_flag(_is_dual_stack_router);
	std::string p_buf1;
	_Response.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::Xgetpeername(unsigned short _sport)
{
	xia::XSocketMsg _xsm;
	_xsm.set_type(xia::XGETPEERNAME);
	xia::X_GetPeername_Msg *_msg = _xsm.mutable_x_getpeername();

	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	_msg->set_dag(daginfo->dst_path.unparse().c_str());

	std::string p_buf1;
	_xsm.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}
					

void XTRANSPORT::Xgetsockname(unsigned short _sport)
{
	xia::XSocketMsg _xsm;
	_xsm.set_type(xia::XGETSOCKNAME);
	xia::X_GetSockname_Msg *_msg = _xsm.mutable_x_getsockname();

	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	_msg->set_dag(daginfo->src_path.unparse().c_str());

	std::string p_buf1;
	_xsm.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}


void XTRANSPORT::Xsend(unsigned short _sport, WritablePacket *p_in)
{
	//click_chatter("Xsend on %d\n", _sport);

	xia::X_Send_Msg *x_send_msg = xia_socket_msg.mutable_x_send();

	String pktPayload(x_send_msg->payload().c_str(), x_send_msg->payload().size());

	int pktPayloadSize = pktPayload.length();
	char payload[16384];
	memcpy(payload, pktPayload.c_str(), pktPayload.length());
	//click_chatter("pkt %s port %d", pktPayload.c_str(), _sport);
	//printf("XSEND: %d bytes from (%d)\n", pktPayloadSize, _sport);

	//Find DAG info for that stream
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);
	if(daginfo && daginfo->sock_type == SOCK_RAW) {
		struct click_xia *xiah = reinterpret_cast<struct click_xia *>(payload);
		click_chatter("Xsend: xiah->ver = %d", xiah->ver);
		click_chatter("Xsend: xiah->nxt = %d", xiah->nxt);
		click_chatter("Xsend: xiah->plen = %d", xiah->plen);
		click_chatter("Xsend: xiah->hlim = %d", xiah->hlim);
		click_chatter("Xsend: xiah->dnode = %d", xiah->dnode);
		click_chatter("Xsend: xiah->snode = %d", xiah->snode);
		click_chatter("Xsend: xiah->last = %d", xiah->last);
		int total_nodes = xiah->dnode + xiah->snode;
		for(int i=0;i<total_nodes;i++) {
			uint8_t id[20];
			char hex_string[41];
			bzero(hex_string, 41);
			memcpy(id, xiah->node[i].xid.id, 20);
			for(int j=0;j<20;j++) {
				sprintf(&hex_string[2*j], "%02x", (unsigned int)id[j]);
			}
			char type[10];
			bzero(type, 10);
			switch (htonl(xiah->node[i].xid.type)) {
				case CLICK_XIA_XID_TYPE_AD:
					strcpy(type, "AD");
					break;
				case CLICK_XIA_XID_TYPE_HID:
					strcpy(type, "HID");
					break;
				case CLICK_XIA_XID_TYPE_SID:
					strcpy(type, "SID");
					break;
				case CLICK_XIA_XID_TYPE_CID:
					strcpy(type, "CID");
					break;
				case CLICK_XIA_XID_TYPE_IP:
					strcpy(type, "4ID");
					break;
				default:
					sprintf(type, "%d", xiah->node[i].xid.type);
			};
			click_chatter("Xsend: %s:%s", type, hex_string);
		}

		XIAHeader xiaheader(xiah);
		XIAHeaderEncap xiahencap(xiaheader);
		XIAPath dst_path = xiaheader.dst_path();
		click_chatter("Xsend: Sending RAW packet to:%s:", dst_path.unparse().c_str());
		size_t headerlen = xiaheader.hdr_size();
		char *pktcontents = &payload[headerlen];
		int pktcontentslen = pktPayloadSize - headerlen;
		click_chatter("Xsend: Packet size without XIP header:%d", pktcontentslen);

		WritablePacket *p = WritablePacket::make(p_in->headroom() + 1, (const void*)pktcontents, pktcontentslen, p_in->tailroom());
		p = xiahencap.encap(p, false);

		output(NETWORK_PORT).push(p);
		return;
	}
	if (daginfo && daginfo->isConnected) {

		//Recalculate source path
		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse();
		//Make source DAG _local_addr:SID
		String dagstr = daginfo->src_path.unparse_re();

		//Client Mobility...
		if (dagstr.length() != 0 && dagstr != str_local_addr) {
			//Moved!
			// 1. Update 'daginfo->src_path'
			daginfo->src_path.parse_re(str_local_addr);
		}
	
		// Case of initial binding to only SID
		if(daginfo->full_src_dag == false) {
			daginfo->full_src_dag = true;
			String str_local_addr = _local_addr.unparse_re();
			XID front_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
			String xid_string = front_xid.unparse();
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
			daginfo->src_path.parse_re(str_local_addr);
		}

		//Add XIA headers
		XIAHeaderEncap xiah;
		xiah.set_nxt(CLICK_XIA_NXT_TRN);
		xiah.set_last(LAST_NODE_DEFAULT);
		xiah.set_hlim(hlim.get(_sport));
		xiah.set_dst_path(daginfo->dst_path);
		xiah.set_src_path(daginfo->src_path);
		xiah.set_plen(pktPayloadSize);

		if (DEBUG)
			click_chatter("XSEND: (%d) sent packet to %s, from %s\n", _sport, daginfo->dst_path.unparse_re().c_str(), daginfo->src_path.unparse_re().c_str());

		WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_send_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

		WritablePacket *p = NULL;

		//Add XIA Transport headers
		TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDATAHeader(daginfo->next_seqnum, daginfo->ack_num, 0 ); // #seq, #ack, length
		p = thdr->encap(just_payload_part);

		thdr->update();
		xiah.set_plen(pktPayloadSize + thdr->hlen()); // XIA payload = transport header + transport-layer data

		p = xiah.encap(p, false);

		delete thdr;

		// Store the packet into buffer
		WritablePacket *tmp = daginfo->sent_pkt[daginfo->next_seqnum % MAX_WIN_SIZE];
		daginfo->sent_pkt[daginfo->next_seqnum % MAX_WIN_SIZE] = copy_packet(p, daginfo);
		if (tmp)
			tmp->kill();

		// printf("XSEND: SENT DATA at (%s) seq=%d \n\n", dagstr.c_str(), daginfo->next_seqnum%MAX_WIN_SIZE);

		daginfo->next_seqnum++;

		// Set timer
		daginfo->timer_on = true;
		daginfo->dataack_waiting = true;
		daginfo->num_retransmit_tries = 0;
		daginfo->expiry = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);

		if (! _timer.scheduled() || _timer.expiry() >= daginfo->expiry )
			_timer.reschedule_at(daginfo->expiry);

		portToDAGinfo.set(_sport, *daginfo);

		//click_chatter("Sent packet to network");
		XIAHeader xiah1(p);
		String pld((char *)xiah1.payload(), xiah1.plen());
		//printf("\n\n (%s) send (timer set at %f) =%s  len=%d \n\n", (_local_addr.unparse()).c_str(), (daginfo->expiry).doubleval(), pld.c_str(), xiah1.plen());
		output(NETWORK_PORT).push(p);

		// REMOVED STATUS RETURNS AS WE RAN INTO SEQUENCING ERRORS
		// WHERE IT INTERLEAVED WITH RECEIVE PACKETS
		// (for Ack purpose) Reply with a packet with the destination port=source port
//				ReturnResult(_sport, xia::XSEND);

	} else {
	
//				ReturnResult(_sport, xia::XSEND, -1, ENOTCONN);
		click_chatter("Not 'connect'ed: you may need to use 'sendto()'");
	}
}

void XTRANSPORT::Xsendto(unsigned short _sport, WritablePacket *p_in)
{
	xia::X_Sendto_Msg *x_sendto_msg = xia_socket_msg.mutable_x_sendto();

	String dest(x_sendto_msg->ddag().c_str());
	String pktPayload(x_sendto_msg->payload().c_str(), x_sendto_msg->payload().size());
	int pktPayloadSize = pktPayload.length();
	//click_chatter("\n SENDTO ddag:%s, payload:%s, length=%d\n",xia_socket_msg.ddag().c_str(), xia_socket_msg.payload().c_str(), pktPayloadSize);

	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	if(!daginfo) {
		//No local SID bound yet, so bind one
		daginfo = new DAGinfo();
	}

	if (daginfo->initialized == false) {
		daginfo->initialized = true;
		daginfo->full_src_dag = true;
		daginfo->port = _sport;
		String str_local_addr = _local_addr.unparse_re();

		char xid_string[50];
		random_xid("SID", xid_string);
//		String rand(click_random(1000000, 9999999));
//		String xid_string = "SID:20000ff00000000000000000000000000" + rand;
		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID

		daginfo->src_path.parse_re(str_local_addr);

		daginfo->last = LAST_NODE_DEFAULT;
		daginfo->hlim = hlim.get(_sport);

		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());

		XIDtoPort.set(source_xid, _sport); //Maybe change the mapping to XID->DAGinfo?
		addRoute(source_xid);
	}

	// Case of initial binding to only SID
	if(daginfo->full_src_dag == false) {
		daginfo->full_src_dag = true;
		String str_local_addr = _local_addr.unparse_re();
		XID front_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		String xid_string = front_xid.unparse();
		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
		daginfo->src_path.parse_re(str_local_addr);
	}


	if(daginfo->src_path.unparse_re().length() != 0) {
		//Recalculate source path
		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
		daginfo->src_path.parse(str_local_addr);
	}

	portToDAGinfo.set(_sport, *daginfo);

	daginfo = portToDAGinfo.get_pointer(_sport);

//			if (DEBUG)
//				click_chatter("sent packet from %s, to %s\n", daginfo->src_path.unparse_re().c_str(), dest.c_str());

	//Add XIA headers
	XIAHeaderEncap xiah;

	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(hlim.get(_sport));
	xiah.set_dst_path(dst_path);
	xiah.set_src_path(daginfo->src_path);

	WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_sendto_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

	WritablePacket *p = NULL;

	// FIXME: shouldn't be a raw number
	if (daginfo->sock_type == 3) {
		xiah.set_nxt(nxt_xport.get(_sport));

		xiah.set_plen(pktPayloadSize);
		p = xiah.encap(just_payload_part, false);

	} else {
		xiah.set_nxt(CLICK_XIA_NXT_TRN);
		xiah.set_plen(pktPayloadSize);

		//p = xiah.encap(just_payload_part, true);
		//printf("\n\nSEND: %s ---> %s\n\n", daginfo->src_path.unparse_re().c_str(), dest.c_str());
		//printf("payload=%s len=%d \n\n", x_sendto_msg->payload().c_str(), pktPayloadSize);

		//Add XIA Transport headers
		TransportHeaderEncap *thdr = TransportHeaderEncap::MakeDGRAMHeader(0); // length
		p = thdr->encap(just_payload_part);

		thdr->update();
		xiah.set_plen(pktPayloadSize + thdr->hlen()); // XIA payload = transport header + transport-layer data

		p = xiah.encap(p, false);
		delete thdr;
	}

	output(NETWORK_PORT).push(p);

	// removed due to multi peer collision problem
	// (for Ack purpose) Reply with a packet with the destination port=source port
//				ReturnResult(_sport, xia::XSENDTO);
}

void XTRANSPORT::XrequestChunk(unsigned short _sport, WritablePacket *p_in)
{
	xia::X_Requestchunk_Msg *x_requestchunk_msg = xia_socket_msg.mutable_x_requestchunk();

	String pktPayload(x_requestchunk_msg->payload().c_str(), x_requestchunk_msg->payload().size());
	int pktPayloadSize = pktPayload.length();

	// send CID-Requests

	for (int i = 0; i < x_requestchunk_msg->dag_size(); i++) {
		String dest = x_requestchunk_msg->dag(i).c_str();
		//printf("CID-Request for %s  (size=%d) \n", dest.c_str(), dag_size);
		//printf("\n\n (%s) hi 3 \n\n", (_local_addr.unparse()).c_str());
		XIAPath dst_path;
		dst_path.parse(dest);

		//Find DAG info for this DGRAM
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

		if(!daginfo) {
			//No local SID bound yet, so bind one
			daginfo = new DAGinfo();
		}

		if (daginfo->initialized == false) {
			daginfo->initialized = true;
			daginfo->full_src_dag = true;
			daginfo->port = _sport;
			String str_local_addr = _local_addr.unparse_re();

			char xid_string[50];
			random_xid("SID", xid_string);
//			String rand(click_random(1000000, 9999999));
//			String xid_string = "SID:20000ff00000000000000000000000000" + rand;
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID

			daginfo->src_path.parse_re(str_local_addr);

			daginfo->last = LAST_NODE_DEFAULT;
			daginfo->hlim = hlim.get(_sport);

			XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());

			XIDtoPort.set(source_xid, _sport); //Maybe change the mapping to XID->DAGinfo?
			addRoute(source_xid);

		}
	
		// Case of initial binding to only SID
		if(daginfo->full_src_dag == false) {
			daginfo->full_src_dag = true;
			String str_local_addr = _local_addr.unparse_re();
			XID front_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
			String xid_string = front_xid.unparse();
			str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
			daginfo->src_path.parse_re(str_local_addr);
		}
	
		if(daginfo->src_path.unparse_re().length() != 0) {
			//Recalculate source path
			XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
			String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
			daginfo->src_path.parse(str_local_addr);
		}

		portToDAGinfo.set(_sport, *daginfo);

		daginfo = portToDAGinfo.get_pointer(_sport);

		if (DEBUG)
			click_chatter("sent packet to %s, from %s\n", dest.c_str(), daginfo->src_path.unparse_re().c_str());

		//Add XIA headers
		XIAHeaderEncap xiah;
		xiah.set_nxt(CLICK_XIA_NXT_CID);
		xiah.set_last(LAST_NODE_DEFAULT);
		xiah.set_hlim(hlim.get(_sport));
		xiah.set_dst_path(dst_path);
		xiah.set_src_path(daginfo->src_path);
		xiah.set_plen(pktPayloadSize);

		WritablePacket *just_payload_part = WritablePacket::make(p_in->headroom() + 1, (const void*)x_requestchunk_msg->payload().c_str(), pktPayloadSize, p_in->tailroom());

		WritablePacket *p = NULL;

		//Add Content header
		ContentHeaderEncap *chdr = ContentHeaderEncap::MakeRequestHeader();
		p = chdr->encap(just_payload_part);
		p = xiah.encap(p, true);
		delete chdr;

		XID	source_sid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		XID	destination_cid = dst_path.xid(dst_path.destination_node());

		XIDpair xid_pair;
		xid_pair.set_src(source_sid);
		xid_pair.set_dst(destination_cid);

		// Map the src & dst XID pair to source port
		XIDpairToPort.set(xid_pair, _sport);

		// Store the packet into buffer
		WritablePacket *copy_req_pkt = copy_cid_req_packet(p, daginfo);
		daginfo->XIDtoCIDreqPkt.set(destination_cid, copy_req_pkt);

		// Set the status of CID request
		daginfo->XIDtoStatus.set(destination_cid, WAITING_FOR_CHUNK);

		// Set the status of ReadCID reqeust
		daginfo->XIDtoReadReq.set(destination_cid, false);

		// Set timer
		Timestamp cid_req_expiry  = Timestamp::now() + Timestamp::make_msec(_ackdelay_ms);
		daginfo->XIDtoExpiryTime.set(destination_cid, cid_req_expiry);
		daginfo->XIDtoTimerOn.set(destination_cid, true);

		if (! _timer.scheduled() || _timer.expiry() >= cid_req_expiry )
			_timer.reschedule_at(cid_req_expiry);

		portToDAGinfo.set(_sport, *daginfo);

		output(NETWORK_PORT).push(p);
	}
}

void XTRANSPORT::XgetChunkStatus(unsigned short _sport)
{
	xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg = xia_socket_msg.mutable_x_getchunkstatus();

	int numCids = x_getchunkstatus_msg->dag_size();
	String pktPayload(x_getchunkstatus_msg->payload().c_str(), x_getchunkstatus_msg->payload().size());

	// send CID-Requests
	for (int i = 0; i < numCids; i++) {
		String dest = x_getchunkstatus_msg->dag(i).c_str();
		//printf("CID-Request for %s  (size=%d) \n", dest.c_str(), dag_size);
		//printf("\n\n (%s) hi 3 \n\n", (_local_addr.unparse()).c_str());
		XIAPath dst_path;
		dst_path.parse(dest);

		//Find DAG info for this DGRAM
		DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

		XID	destination_cid = dst_path.xid(dst_path.destination_node());

		// Check the status of CID request
		HashTable<XID, int>::iterator it;
		it = daginfo->XIDtoStatus.find(destination_cid);

		if(it != daginfo->XIDtoStatus.end()) {
			// There is an entry
			int status = it->second;

			if(status == WAITING_FOR_CHUNK) {
				x_getchunkstatus_msg->add_status("WAITING");

			} else if(status == READY_TO_READ) {
				x_getchunkstatus_msg->add_status("READY");

			} else if(status == INVALID_HASH) {
				x_getchunkstatus_msg->add_status("INVALID_HASH");

			} else if(status == REQUEST_FAILED) {
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

	std::string p_buf;
	xia_socket_msg.SerializeToString(&p_buf);

	WritablePacket *reply = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::XreadChunk(unsigned short _sport)
{
	
	click_chatter(">>READ chunk message from API %d\n", _sport);
	
	
	xia::X_Readchunk_Msg *x_readchunk_msg = xia_socket_msg.mutable_x_readchunk();

	String dest = x_readchunk_msg->dag().c_str();
	WritablePacket *copy;
	//printf("CID-Request for %s  (size=%d) \n", dest.c_str(), dag_size);
	//printf("\n\n (%s) hi 3 \n\n", (_local_addr.unparse()).c_str());
	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	XID	destination_cid = dst_path.xid(dst_path.destination_node());

	// Update the status of ReadCID reqeust
	daginfo->XIDtoReadReq.set(destination_cid, true);
	portToDAGinfo.set(_sport, *daginfo);

	// Check the status of CID request
	HashTable<XID, int>::iterator it;
	it = daginfo->XIDtoStatus.find(destination_cid);

	if(it != daginfo->XIDtoStatus.end()) {
		// There is an entry
		int status = it->second;

		if (status != READY_TO_READ  &&
			status != INVALID_HASH) {
			// Do nothing

		} else {
			// Send the buffered pkt to upper layer

			daginfo->XIDtoReadReq.set(destination_cid, false);
			portToDAGinfo.set(_sport, *daginfo);

			HashTable<XID, WritablePacket*>::iterator it2;
			it2 = daginfo->XIDtoCIDresponsePkt.find(destination_cid);
			copy = copy_cid_response_packet(it2->second, daginfo);

			XIAHeader xiah(copy->xia_header());

			//Unparse dag info
			String src_path = xiah.src_path().unparse();

			xia::XSocketMsg xia_socket_msg;
			xia_socket_msg.set_type(xia::XREADCHUNK);
			xia::X_Readchunk_Msg *x_readchunk_msg = xia_socket_msg.mutable_x_readchunk();
			x_readchunk_msg->set_dag(src_path.c_str());
			x_readchunk_msg->set_payload((const char *)xiah.payload(), xiah.plen());

			std::string p_buf;
			xia_socket_msg.SerializeToString(&p_buf);

			WritablePacket *p2 = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);

			//printf("FROM CACHE. data length = %d  \n", str.length());
			if (DEBUG)
				click_chatter("Sent packet to socket: sport %d dport %d", _sport, _sport);
			
			
			//TODO: remove
			click_chatter(">>send chunk to API after read %d\n", _sport);
			output(API_PORT).push(UDPIPPrep(p2, _sport));

			it2->second->kill();
			daginfo->XIDtoCIDresponsePkt.erase(it2);

			portToDAGinfo.set(_sport, *daginfo);
		}
	}

}

void XTRANSPORT::XremoveChunk(unsigned short _sport)
{
	xia::X_Removechunk_Msg *x_rmchunk_msg = xia_socket_msg.mutable_x_removechunk();

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

	if (DEBUG) {
		click_chatter("sent remove cid packet to cache");
	}
	output(CACHE_PORT).push(p);

	// (for Ack purpose) Reply with a packet with the destination port=source port
	xia::XSocketMsg _socketResponse;
	_socketResponse.set_type(xia::XREMOVECHUNK);
	xia::X_Removechunk_Msg *_msg = _socketResponse.mutable_x_removechunk();
	_msg->set_contextid(contextID);
	_msg->set_cid(src.c_str());
	_msg->set_status(0);

	std::string p_buf1;
	_socketResponse.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}

void XTRANSPORT::XputChunk(unsigned short _sport)
{
	
	
	click_chatter(">>putchunk message from API %d\n", _sport);
	
	
	
	xia::X_Putchunk_Msg *x_putchunk_msg = xia_socket_msg.mutable_x_putchunk();
//			int hasCID = x_putchunk_msg->hascid();
	int32_t contextID = x_putchunk_msg->contextid();
	int32_t ttl = x_putchunk_msg->ttl();
	int32_t cacheSize = x_putchunk_msg->cachesize();
	int32_t cachePolicy = x_putchunk_msg->cachepolicy();

	String pktPayload(x_putchunk_msg->payload().c_str(), x_putchunk_msg->payload().size());
	String src;

	/* Computes SHA1 Hash if user does not supply it */
	char hexBuf[3];
	int i = 0;
	SHA1_ctx sha_ctx;
	unsigned char digest[HASH_KEYSIZE];
	SHA1_init(&sha_ctx);
	SHA1_update(&sha_ctx, (unsigned char *)pktPayload.c_str() , pktPayload.length() );
	SHA1_final(digest, &sha_ctx);
	for(i = 0; i < HASH_KEYSIZE; i++) {
		sprintf(hexBuf, "%02x", digest[i]);
		src.append(const_cast<char *>(hexBuf), 2);
	}

	if(DEBUG) {
		click_chatter("ctxID=%d, length=%d, ttl=%d cid=%s\n",
					  contextID, x_putchunk_msg->payload().size(), ttl, src.c_str());
	}

	//append local address before CID
	String str_local_addr = _local_addr.unparse_re();
	str_local_addr = "RE " + str_local_addr + " CID:" + src;
	XIAPath src_path;
	src_path.parse(str_local_addr);

	if(DEBUG) {
		click_chatter("DAG: %s\n", str_local_addr.c_str());
	}

	/*TODO: The destination dag of the incoming packet is local_addr:XID
	 * Thus the cache thinks it is destined for local_addr and delivers to socket
	 * This must be ignored. Options
	 * 1. Use an invalid SID
	 * 2. The cache should only store the CID responses and not forward them to
	 *	local_addr when the source and the destination HIDs are the same.
	 * 3. Use the socket SID on which putCID was issued. This will
	 *	result in a reply going to the same socket on which the putCID was issued.
	 *	Use the response to return 1 to the putCID call to indicate success.
	 *	Need to add daginfo/ephemeral SID generation for this to work.
	 * 4. Special OPCODE in content extension header and treat it specially in content module (done below)
	 */

	//Add XIA headers
	XIAHeaderEncap xiah;
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(hlim.get(_sport));
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

	if (DEBUG)
		click_chatter("sent packet to cache");
	
	output(CACHE_PORT).push(p);

	// (for Ack purpose) Reply with a packet with the destination port=source port
	struct timeval timestamp;
	gettimeofday(&timestamp, NULL);
	xia::XSocketMsg _socketResponse;
	_socketResponse.set_type(xia::XPUTCHUNK);
	xia::X_Putchunk_Msg *_msg = _socketResponse.mutable_x_putchunk();
	_msg->set_contextid(contextID);
	_msg->set_cid(src.c_str());
	_msg->set_ttl(ttl);
	_msg->set_timestamp(timestamp.tv_sec);
//	_msg->set_hascid(1);
	_msg->set_cachepolicy(0);
	_msg->set_cachesize(0);
	_msg->set_payload(x_putchunk_msg->payload().c_str(), x_putchunk_msg->payload().size());

	std::string p_buf1;
	_socketResponse.SerializeToString(&p_buf1);
	WritablePacket *reply = WritablePacket::make(256, p_buf1.c_str(), p_buf1.size(), 0);
	output(API_PORT).push(UDPIPPrep(reply, _sport));
}




void XTRANSPORT::XpushChunkto(unsigned short _sport, WritablePacket *p_in)
{
	xia::X_Pushchunkto_Msg *x_pushchunkto_msg= xia_socket_msg.mutable_x_pushchunkto();

	
	int32_t contextID = x_pushchunkto_msg->contextid();
	int32_t ttl = x_pushchunkto_msg->ttl();
	int32_t cacheSize = x_pushchunkto_msg->cachesize();
	int32_t cachePolicy = x_pushchunkto_msg->cachepolicy();
	
	String pktPayload(x_pushchunkto_msg->payload().c_str(), x_pushchunkto_msg->payload().size());
	int pktPayloadSize = pktPayload.length();
	

	// send CID-Requests

	String dest = x_pushchunkto_msg->ddag().c_str();
	//printf("CID-Request for %s  (size=%d) \n", dest.c_str(), dag_size);
	//printf("\n\n (%s) hi 3 \n\n", (_local_addr.unparse()).c_str());
	XIAPath dst_path;
	dst_path.parse(dest);

	//Find DAG info for this DGRAM
	DAGinfo *daginfo = portToDAGinfo.get_pointer(_sport);

	if(!daginfo) {
		//No local SID bound yet, so bind one
		daginfo = new DAGinfo();
	}

	if (daginfo->initialized == false) {
		daginfo->initialized = true;
		daginfo->full_src_dag = true;
		daginfo->port = _sport;
		String str_local_addr = _local_addr.unparse_re();
		

		
		//TODO: AD->HID->SID->CID We can add SID here (AD->HID->SID->CID) if SID is passed or generate randomly
		// Contentmodule forwarding needs to be fixed to get this to work. (wrong comparison there)
		// Since there is no way to dictate policy to cache which content to accept right now this wasn't added.
// 		char xid_string[50];
// 		random_xid("SID", xid_string);
//		String rand(click_random(1000000, 9999999));
//		String xid_string = "SID:20000ff00000000000000000000000000" + rand;
// 		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID

		daginfo->src_path.parse_re(str_local_addr);

		daginfo->last = LAST_NODE_DEFAULT;
		daginfo->hlim = hlim.get(_sport);

		XID source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());

		XIDtoPort.set(source_xid, _sport); //Maybe change the mapping to XID->DAGinfo?
		addRoute(source_xid);

	}

	// Case of initial binding to only SID
	if(daginfo->full_src_dag == false) {
		daginfo->full_src_dag = true;
		String str_local_addr = _local_addr.unparse_re();
		XID front_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		String xid_string = front_xid.unparse();
		str_local_addr = str_local_addr + " " + xid_string; //Make source DAG _local_addr:SID
		printf("str_local_addr: %s", str_local_addr.c_str() );
		daginfo->src_path.parse_re(str_local_addr);
	}

	if(daginfo->src_path.unparse_re().length() != 0) {
		//Recalculate source path
		XID	source_xid = daginfo->src_path.xid(daginfo->src_path.destination_node());
		String str_local_addr = _local_addr.unparse_re() + " " + source_xid.unparse(); //Make source DAG _local_addr:SID
		click_chatter("str_local_addr: %s", str_local_addr.c_str() );
		daginfo->src_path.parse(str_local_addr);
	}

	portToDAGinfo.set(_sport, *daginfo);

	daginfo = portToDAGinfo.get_pointer(_sport);

	if (DEBUG)
		click_chatter("sent packet to %s, from %s\n", dest.c_str(), daginfo->src_path.unparse_re().c_str());


	click_chatter("PUSHCID: %s",x_pushchunkto_msg->cid().c_str());
	String src(x_pushchunkto_msg->cid().c_str(), x_pushchunkto_msg->cid().size());
	//append local address before CID
	String cid_str_local_addr = daginfo->src_path.unparse_re();
	cid_str_local_addr = "RE " + cid_str_local_addr + " CID:" + src;
	XIAPath cid_src_path;
	cid_src_path.parse(cid_str_local_addr);
	click_chatter("cid_local_addr: %s", cid_str_local_addr.c_str() );
	
	
	//Add XIA headers	
	XIAHeaderEncap xiah;
	xiah.set_nxt(CLICK_XIA_NXT_CID);
	xiah.set_last(LAST_NODE_DEFAULT);
	xiah.set_hlim(hlim.get(_sport));
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
	

// 	XID	source_sid = daginfo->src_path.xid(daginfo->src_path.destination_node());
// 	XID	destination_cid = dst_path.xid(dst_path.destination_node());
	//FIXME this is wrong
	XID	source_cid = cid_src_path.xid(cid_src_path.destination_node());
// 	XID	source_cid = daginfo->src_path.xid(cid_src_path.destination_node());
	XID	destination_sid = dst_path.xid(dst_path.destination_node());

	XIDpair xid_pair;
	xid_pair.set_src(source_cid);
	xid_pair.set_dst(destination_sid);

	// Map the src & dst XID pair to source port
	XIDpairToPort.set(xid_pair, _sport);


	portToDAGinfo.set(_sport, *daginfo);

	output(NETWORK_PORT).push(p);
}


CLICK_ENDDECLS

EXPORT_ELEMENT(XTRANSPORT)
ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(XIAContentModule)
ELEMENT_MT_SAFE(XTRANSPORT)
ELEMENT_LIBS(-lcrypto -lssl)
