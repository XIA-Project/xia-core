#ifndef XIA_OVERLAY_SOCKET_HH
#define XIA_OVERLAY_SOCKET_HH

#include <click/element.hh>
#include "../userlevel/socket.hh"

CLICK_DECLS

/*
 * Allow XIA to run as an overlay on UDP/IP
 */
class XIAOverlaySocket : public Socket {
	public:
	const char *class_name() const { return "XIAOverlaySocket"; }
	virtual int write_packet(Packet*);
};

CLICK_ENDDECLS
#endif // XIA_OVERLAY_SOCKET_HH
