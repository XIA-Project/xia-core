#include <click/config.h>
#include "xiapingsource.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAPingSource::XIAPingSource()
    : _timer(this)
{
    _count = 0;
}

XIAPingSource::~XIAPingSource()
{
}

int
XIAPingSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _interval = 1000;

    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   "DST", cpkP+cpkM, cpXIAPath, &_dst_path,
                   "INTERVAL", 0, cpSecondsAsMilli, &_interval,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XIAPingSource::initialize(ErrorHandler *)
{
    _timer.initialize(this);
    _timer.schedule_after_msec(_interval);

    return 0;
}

void
XIAPingSource::run_timer(Timer *) {
    _timer.reschedule_after_msec(_interval);

    WritablePacket *p = Packet::make(256, NULL, 8, 0);
    
    *(uint32_t*)(p->data() + 0) = _count++;
    *(uint32_t*)(p->data() + 4) = 0;

    XIAHeaderEncap encap;
    encap.set_nxt(100);   // PING
    encap.set_dst_path(_dst_path);
    encap.set_src_path(_src_path);

    output(0).push(encap.encap(p));
}

void
XIAPingSource::push(int, Packet *p)
{
    XIAHeader hdr(p);

    switch (hdr.nxt()) {
    case 100:
        // PING
        click_chatter("ignoring PING at XIAPingSource\n");
        break;

    case 101:
        // PONG
        if (hdr.plen() != 8)
            click_chatter("invalid PONG message length\n");
        else
            click_chatter("PONG received; client seq = %u, server seq = %u\n",
                    *(uint32_t*)(hdr.payload() + 0), *(uint32_t*)(hdr.payload() + 4));
        break;

    case 102:
        // UPDATE
        {
            XIAPath new_path;
            new_path.parse_node(
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload()),
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload() + hdr.plen())
            );
            click_chatter("updating XIAPingSource with new address %s\n", new_path.unparse(this).c_str());
            _dst_path = new_path;
            break;
        }

    default:
        click_chatter("invalid message type\n");
        break;
    }

    p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPingSource)
ELEMENT_MT_SAFE(XIAPingSource)
