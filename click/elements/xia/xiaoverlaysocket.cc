#include <click/config.h>
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>

#include "xiaoverlaysocket.hh"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

CLICK_DECLS

void
XIAOverlaySocket::selected(int fd, int)
{
  int len;
  union { struct sockaddr_in in; struct sockaddr_un un; } from;
  socklen_t from_len = sizeof(from);
  bool allow;

  if (noutputs()) {
    // accept new connections
    if (_socktype == SOCK_STREAM && !_client && _active < 0 && fd == _fd) {
      _active = accept(_fd, (struct sockaddr *)&from, &from_len);

      if (_active < 0) {
	if (errno != EAGAIN)
	  click_chatter("%s: accept: %s", declaration().c_str(), strerror(errno));
	return;
      }

      if (_family == AF_INET) {
	allow = allowed(IPAddress(from.in.sin_addr));

	if (_verbose)
	  click_chatter("%s: %s connection %d from %s:%d", declaration().c_str(),
			allow ? "opened" : "denied",
			_active, IPAddress(from.in.sin_addr).unparse().c_str(), ntohs(from.in.sin_port));

	if (!allow) {
	  close(_active);
	  _active = -1;
	  return;
	}
      } else {
	if (_verbose)
	  click_chatter("%s: opened connection %d from %s", declaration().c_str(), _active, from.un.sun_path);
      }

      fcntl(_active, F_SETFL, O_NONBLOCK);
      fcntl(_active, F_SETFD, FD_CLOEXEC);

      add_select(_active, SELECT_READ | SELECT_WRITE);
    }

    // read data from socket
    if (!_rq)
      _rq = Packet::make(_headroom, 0, _snaplen, 0);
    if (_rq) {
      if (_socktype == SOCK_STREAM)
	len = read(_active, _rq->data(), _rq->length());
      else if (_client)
	len = recv(_active, _rq->data(), _rq->length(), MSG_TRUNC);
      else {
	// datagram server, find out who we are talking to
	len = recvfrom(_active, _rq->data(), _rq->length(), MSG_TRUNC, (struct sockaddr *)&from, &from_len);
	printf("XIAOverlaySocket::selected rcvd pkt of len %d\n", len);
	printf("from: %s:%d\n", IPAddress(from.in.sin_addr).unparse().c_str(), ntohs(from.in.sin_port));

	if (_family == AF_INET && !allowed(IPAddress(from.in.sin_addr))) {
	  if (_verbose)
	    click_chatter("%s: dropped datagram from %s:%d", declaration().c_str(),
			  IPAddress(from.in.sin_addr).unparse().c_str(), ntohs(from.in.sin_port));
	  len = -1;
	  errno = EAGAIN;
	} else if (len > 0) {
	  memcpy(&_remote, &from, from_len);
	  _remote_len = from_len;
	  _rq->set_src_ip_anno(IPAddress(_remote.in.sin_addr));
	  SET_SRC_PORT_ANNO(_rq, _remote.in.sin_port);
	  printf("XIAOverlaySocket::pkt src: %s\n",
			  IPAddress(_rq->src_ip_anno()).unparse().c_str());
	  printf("XIAOverlaySocket::pkt port: %d\n",
			  ntohs(SRC_PORT_ANNO(_rq)));
	}
      }

      // this segment OK
      if (len > 0) {
	if (len > _snaplen) {
	  // truncate packet to max length (should never happen)
	  assert(_rq->length() == (uint32_t)_snaplen);
	  SET_EXTRA_LENGTH_ANNO(_rq, len - _snaplen);
	} else {
	  // trim packet to actual length
	  _rq->take(_snaplen - len);
	}

	// set timestamp
	if (_timestamp)
	  _rq->timestamp_anno().assign_now();

	// push packet
	printf("XIAOverlaySocket::selected pushing packet to XIA router\n");
	output(0).push(_rq);
	_rq = 0;
      }

      // connection terminated or fatal error
      else if (len == 0 || errno != EAGAIN) {
	if (errno != EAGAIN && _verbose)
	  click_chatter("%s: %s", declaration().c_str(), strerror(errno));
	close_active();
	return;
      }
    }
  }

  if (ninputs() && input_is_pull(0))
    run_task(0);
}

int
XIAOverlaySocket::write_packet(Packet *p)
{
    int len;


    assert(_active >= 0);

    while (p->length()) {
		struct sockaddr_in dest;
		dest.sin_family = AF_INET;
		dest.sin_port = DST_PORT_ANNO(p);
		dest.sin_addr.s_addr = p->dst_ip_anno().addr();
		if(dest.sin_addr.s_addr == 0) {
			break;
		}
		printf("XIAOverlaySocket: sending a packet to: %s:%d\n",
				inet_ntoa(dest.sin_addr), ntohs(dest.sin_port));

		const struct click_xia* hdr = p->xia_header();
		if(p->data() != (uint8_t*) p->xia_header()) {
			printf("XIAOverlaySocket: ERROR xia hdr != p->data\n");
		}

        if (_socktype != SOCK_DGRAM) {
            click_chatter("XIAOverlaySocket: ERROR: not datagram socket");
			p->kill();
            return -1;
        }
        len = sendto(_active, p->data(), p->length(), 0,
                (struct sockaddr *)&dest, sizeof(dest));
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

void
XIAOverlaySocket::push(int, Packet *p)
{
  fd_set fds;
  int err;

  if (_active >= 0) {
    // block
    do {
      FD_ZERO(&fds);
      FD_SET(_active, &fds);
      err = select(_active + 1, NULL, &fds, NULL, NULL);
    } while (err < 0 && errno == EINTR);

    if (err >= 0) {
      // write
      do {
	err = write_packet(p);
      } while (err < 0 && (errno == ENOBUFS || errno == EAGAIN));
    }

    if (err < 0) {
      if (_verbose)
	click_chatter("%s: %s, dropping packet", declaration().c_str(), strerror(err));
      p->kill();
    }
  } else
    p->kill();
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
