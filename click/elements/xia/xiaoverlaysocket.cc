#include <click/config.h>
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>

#include "xiaoverlaysocket.hh"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

CLICK_DECLS

int
XIAOverlaySocket::write_packet(Packet *p)
{
	int len;

	assert(_active >= 0);

	while (p->length()) {
		_remote.in.sin_addr = p->dst_ip_anno();
		_remote.in.sin_port = DST_PORT_ANNO(p);
		printf("XIAOverlaySocket: sending a packet to: %s:%d",
				inet_ntoa(_remote.in.sin_addr), ntohs(_remote.in.sin_port));

		if (_socktype != SOCK_DGRAM) {
			click_chatter("ERROR: not datagram socket");
			return -1;
		}
		len = sendto(_active, p->data(), p->length(), 0,
				(struct sockaddr *)&_remote, _remote_len);
		// error
		if (len < 0) {
			// out of memory or would block
			if (errno == ENOBUFS || errno == EAGAIN) {
				return -1;
			} else if (errno == EINTR) {
				continue;
			} else {
				if(_verbose) {
					click_chatter("%s: %s", declaration().c_str(),
							strerror(errno));
				}
				close_active();
				break;
			}
		} else {
			// this segment OK
			p->pull(len);
		}
	}
	p->kill();
	return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel Socket)
EXPORT_ELEMENT(XIAOverlaySocket)
