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
#include "xiacontentmodule.hh"
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
#endif

// FIXME: put these in a std location that can be found by click and the API
#define XOPT_HLIM 0x07001
#define XOPT_NEXT_PROTO 0x07002

#ifndef DEBUG
#define DEBUG 0
#endif


#define UNUSED(x) ((void)(x))


#define REQUEST_FAILED		0x00000001
#define WAITING_FOR_CHUNK	0x00000002
#define READY_TO_READ		0x00000004
#define INVALID_HASH		0x00000008

#define HASH_KEYSIZE    20

CLICK_DECLS


class XIAContentModule;   

class XChunk : public sock {
	
public:
	void push(Packet *_p);
	XChunk(XTRANSPORT *transport, unsigned short port);
	XChunk(){};
	// ~XChunk(){}
private:
	/* =========================
	 * XSP/XChunkP Socket states
	 * ========================= */

		bool isConnected;
		bool initialized;
		bool isAcceptSocket;
		bool synack_waiting;
		bool dataack_waiting;
		bool teardown_waiting;

		bool did_poll;
		unsigned polling;

		int num_connect_tries; // number of xconnect tries (Xconnect will fail after MAX_CONNECT_TRIES trials)
		int num_retransmit_tries; // number of times to try resending data packets

  //   	queue<sock*> pending_connection_buf;
		// queue<xia::XSocketMsg*> pendingAccepts; // stores accept messages from API when there are no pending connections
	
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
  
} ;

CLICK_ENDDECLS

#endif
