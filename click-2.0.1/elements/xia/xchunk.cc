#include "../../userlevel/xia.pb.h"
#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <click/vector.hh>

#include <click/xiacontentheader.hh>
#include "xchunk.hh"
#include <click/xiatransportheader.hh>


CLICK_DECLS

XChunk::XChunk(XTRANSPORT *transport, unsigned short port)
	: sock(transport, port, XSOCKET_CHUNK) {}

void XChunk::push(Packet *_p) {
	
}


CLICK_ENDDECLS

EXPORT_ELEMENT(XChunk)
ELEMENT_REQUIRES(userlevel)
ELEMENT_REQUIRES(XIAContentModule)
