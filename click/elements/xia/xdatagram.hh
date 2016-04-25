#ifndef CLICK_XDATAGRAM_HH
#define CLICK_XDATAGRAM_HH

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
#include <clicknet/udp.h>
#include <click/string.hh>
#include <elements/ipsec/sha1_impl.hh>
#include <click/xiatransportheader.hh>
#include "xtransport.hh"

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

CLICK_DECLS


class XIAContentModule;

class XDatagram : public sock {

public:
	XDatagram(XTRANSPORT *transport, unsigned short port);
	XDatagram(){};
	~XDatagram() {};
	int read_from_recv_buf(XSocketMsg *xia_socket_msg);
	void push(WritablePacket *p_in);
private:
	bool should_buffer_received_packet(WritablePacket *p);
	void add_packet_to_recv_buf(WritablePacket *p);
	void check_for_and_handle_pending_recv();

	// // send buffer
	// uint32_t send_buffer_size;
	// uint32_t send_base;				// the sequence # of the oldest unacked packet
	// uint32_t next_send_seqnum;		// the smallest unused sequence # (i.e., the sequence # of the next packet to be sent)
	// uint32_t remote_recv_window;	// num additional *packets* the receiver has room to buffer
	// WritablePacket *send_buffer[MAX_SEND_WIN_SIZE]; // packets we've sent but have not gotten an ACK for

	// /* =========================
	//  * shared tcp/udp receive buffers
	//  * ========================= */
	// WritablePacket *recv_buffer[MAX_RECV_WIN_SIZE]; // packets we've received but haven't delivered to the app
	// uint32_t recv_buffer_size;		// the number of PACKETS we can buffer (received but not delivered to app)
	// uint32_t recv_base;				// sequence # of the oldest received packet not delivered to app
	// uint32_t next_recv_seqnum;		// the sequence # of the next in-order packet we expect to receive
	// int dgram_buffer_start;			// the first undelivered index in the recv buffer (DGRAM only)
	// int dgram_buffer_end;			// the last undelivered index in the recv buffer (DGRAM only)
	// uint32_t recv_buffer_count;		// the number of packets in the buffer (DGRAM only)

} ;

CLICK_ENDDECLS

#endif
