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

    printf("XIAOverlaySocket::write_packet got an overlay packet\n");

    assert(_active >= 0);

    while (p->length()) {
        _remote.in.sin_addr = p->dst_ip_anno();
        _remote.in.sin_port = DST_PORT_ANNO(p);
		if(_remote.in.sin_addr == 0) {
			printf("XIAOverlaySocket::write_packet no addr for pkt\n");
			break;
		}
        printf("XIAOverlaySocket: sending a packet to: %s:%d\n",
                inet_ntoa(_remote.in.sin_addr), ntohs(_remote.in.sin_port));

        if (_socktype != SOCK_DGRAM) {
            click_chatter("XIAOverlaySocket: ERROR: not datagram socket");
			p->kill();
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

bool
XIAOverlaySocket::run_task(Task *)
{
  assert(ninputs() && input_is_pull(0));
  bool any = false;

  if (_active >= 0) {
    Packet *p = 0;
    int err = 0;

    // write as much as we can
    do {
      p = _wq ? _wq : input(0).pull();
      _wq = 0;
      if (p) {
	any = true;
	err = write_packet(p);
      }
    } while (p && err >= 0);

    if (err < 0) {
      // queue packet for writing when socket becomes available
      _wq = p;
      p = 0;
      add_select(_active, SELECT_WRITE);
    } else if (_signal)
      // more pending
      // (can't use fast_reschedule() cause selected() calls this)
      _task.reschedule();
    else
      // wrote all we could and no more pending
      remove_select(_active, SELECT_WRITE);
  }

  // true if we wrote at least one packet
  return any;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel IPRouteTable Socket)
EXPORT_ELEMENT(XIAOverlaySocket)
