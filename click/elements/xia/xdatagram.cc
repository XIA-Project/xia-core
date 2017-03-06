#include "../../userlevel/xia.pb.h"
#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>
#include "xtransport.hh"
#include "xdatagram.hh"
#include "xlog.hh"


CLICK_DECLS

XDatagram::XDatagram(XTRANSPORT *transport, un_abstract port, uint32_t id, int type)
	: sock(transport, port, id, type) {
	memset(send_buffer, 0, MAX_SEND_WIN_SIZE * sizeof(WritablePacket*));
	memset(recv_buffer, 0, MAX_RECV_WIN_SIZE * sizeof(WritablePacket*));
}

void
XDatagram::push(WritablePacket *p_in) {
	// buffer packet if this is a DGRAM socket and we have room
	if (should_buffer_received_packet(p_in)) {
		add_packet_to_recv_buf(p_in);
		interface_id = SRC_PORT_ANNO(p_in);
		if (polling) {
			// tell API we are readable
			get_transport()->ProcessPollEvent(get_id(), POLLIN);
		}
		check_for_and_handle_pending_recv();
	}
}

bool
XDatagram::should_buffer_received_packet(WritablePacket *p) {
	UNUSED(p);
	if (recv_buffer_count < recv_buffer_size) return true;
	else return false;
}

void
XDatagram::add_packet_to_recv_buf(WritablePacket *p) {
	int index = (dgram_buffer_end + 1) % recv_buffer_size;
	dgram_buffer_end = index;
	recv_buffer_count++;
	WritablePacket *p_cpy = p->clone()->uniqueify();
	recv_buffer[index] = p_cpy;
}

void
XDatagram::check_for_and_handle_pending_recv() {

	if (recv_pending) {
		int bytes_returned = read_from_recv_buf(pending_recv_msg);
		get_transport()->ReturnResult(port, pending_recv_msg, bytes_returned);

		recv_pending = false;
		delete pending_recv_msg;
		pending_recv_msg = NULL;
	}
}

int
XDatagram::read_from_recv_buf(XSocketMsg *xia_socket_msg) {

	X_Recvfrom_Msg *x_recvfrom_msg = xia_socket_msg->mutable_x_recvfrom();

	bool peek = x_recvfrom_msg->flags() & MSG_PEEK;

	// Get just the next packet in the recv buffer (we don't return data from more
	// than one packet in case the packets came from different senders). If no
	// packet is available, we indicate to the app that we returned 0 bytes.
	WritablePacket *p = recv_buffer[dgram_buffer_start];

	if (recv_buffer_count > 0 && p) {
		// get different sized packages depending on socket type
		// datagram only wants payload
		// raw wants all headers
		XIAHeader xiah(p->xia_header());
		int data_size;
		String payload;

		switch (sock_type) {
			case SOCK_DGRAM:
			{
				DatagramHeader dhdr(p);
				data_size = dhdr.plen();

				payload = String((const char*)dhdr.payload(), data_size);
				break;
			}

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
			recv_buffer[dgram_buffer_start] = NULL;
			recv_buffer_count--;
			dgram_buffer_start = (dgram_buffer_start + 1) % recv_buffer_size;
		}

		return data_size;
	}
	return -1;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(XDatagram)
ELEMENT_REQUIRES(userlevel)
