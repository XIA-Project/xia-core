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
	XDatagram(XTRANSPORT *transport, unsigned short port, int type);
	XDatagram(){};
	~XDatagram() {};
	int read_from_recv_buf(XSocketMsg *xia_socket_msg);
	void push(WritablePacket *p_in);
private:
	bool should_buffer_received_packet(WritablePacket *p);
	void add_packet_to_recv_buf(WritablePacket *p);
	void check_for_and_handle_pending_recv();
} ;

CLICK_ENDDECLS

#endif
